// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.provider.Settings;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@JNINamespace("media")
class AudioManagerAndroid {
    private static final String TAG = "AudioManagerAndroid";

    // Set to true to enable debug logs. Always check in as false.
    private static final boolean DEBUG = false;

    /** Simple container for device information. */
    private static class AudioDeviceName {
        private final int mId;
        private final String mName;

        private AudioDeviceName(int id, String name) {
            mId = id;
            mName = name;
        }

        @CalledByNative("AudioDeviceName")
        private String id() { return String.valueOf(mId); }

        @CalledByNative("AudioDeviceName")
        private String name() { return mName; }
    }

    // Supported audio device types.
    private static final int DEVICE_INVALID = -1;
    private static final int DEVICE_SPEAKERPHONE = 0;
    private static final int DEVICE_WIRED_HEADSET = 1;
    private static final int DEVICE_EARPIECE = 2;
    private static final int DEVICE_BLUETOOTH_HEADSET = 3;
    private static final int DEVICE_COUNT = 4;

    // Maps audio device types to string values. This map must be in sync
    // with the device types above.
    // TODO(henrika): add support for proper detection of device names and
    // localize the name strings by using resource strings.
    private static final String[] DEVICE_NAMES = new String[] {
        "Speakerphone",
        "Wired headset",    // With or without microphone
        "Headset earpiece", // Only available on mobile phones
        "Bluetooth headset",
    };

    // List of valid device types.
    private static Integer[] VALID_DEVICES = new Integer[] {
        DEVICE_SPEAKERPHONE,
        DEVICE_WIRED_HEADSET,
        DEVICE_EARPIECE,
        DEVICE_BLUETOOTH_HEADSET,
    };

    // The device does not have any audio device.
    static final int STATE_NO_DEVICE_SELECTED = 0;
    // The speakerphone is on and an associated microphone is used.
    static final int STATE_SPEAKERPHONE_ON = 1;
    // The phone's earpiece is on and an associated microphone is used.
    static final int STATE_EARPIECE_ON = 2;
    // A wired headset (with or without a microphone) is plugged in.
    static final int STATE_WIRED_HEADSET_ON = 3;
    // The audio stream is being directed to a Bluetooth headset.
    static final int STATE_BLUETOOTH_ON = 4;
    // We've requested that the audio stream be directed to Bluetooth, but
    // have not yet received a response from the framework.
    static final int STATE_BLUETOOTH_TURNING_ON = 5;
    // We've requested that the audio stream stop being directed to
    // Bluetooth, but have not yet received a response from the framework.
    static final int STATE_BLUETOOTH_TURNING_OFF = 6;
    // TODO(henrika): document the valid state transitions.

    // Use 44.1kHz as the default sampling rate.
    private static final int DEFAULT_SAMPLING_RATE = 44100;
    // Randomly picked up frame size which is close to return value on N4.
    // Return this value when getProperty(PROPERTY_OUTPUT_FRAMES_PER_BUFFER)
    // fails.
    private static final int DEFAULT_FRAME_PER_BUFFER = 256;

    private final AudioManager mAudioManager;
    private final Context mContext;
    private final long mNativeAudioManagerAndroid;

    private boolean mHasBluetoothPermission = false;
    private boolean mIsInitialized = false;
    private boolean mSavedIsSpeakerphoneOn;
    private boolean mSavedIsMicrophoneMute;

    private Integer mAudioDeviceState = STATE_NO_DEVICE_SELECTED;

    // Lock to protect |mAudioDevices| which can be accessed from the main
    // thread and the audio manager thread.
    private final Object mLock = new Object();

    // Contains a list of currently available audio devices.
    private boolean[] mAudioDevices = new boolean[DEVICE_COUNT];

    private final ContentResolver mContentResolver;
    private SettingsObserver mSettingsObserver = null;
    private SettingsObserverThread mSettingsObserverThread = null;
    private int mCurrentVolume;
    private final Object mSettingsObserverLock = new Object();

    // Broadcast receiver for wired headset intent broadcasts.
    private BroadcastReceiver mWiredHeadsetReceiver;

    /** Construction */
    @CalledByNative
    private static AudioManagerAndroid createAudioManagerAndroid(
            Context context,
            long nativeAudioManagerAndroid) {
        return new AudioManagerAndroid(context, nativeAudioManagerAndroid);
    }

    private AudioManagerAndroid(Context context, long nativeAudioManagerAndroid) {
        mContext = context;
        mNativeAudioManagerAndroid = nativeAudioManagerAndroid;
        mAudioManager = (AudioManager)mContext.getSystemService(Context.AUDIO_SERVICE);
        mContentResolver = mContext.getContentResolver();
    }

    /**
     * Saves the initial speakerphone and microphone state.
     * Populates the list of available audio devices and registers receivers
     * for broadcasted intents related to wired headset and bluetooth devices.
     */
    @CalledByNative
    public void init() {
        if (mIsInitialized)
            return;

        synchronized (mLock) {
            for (int i = 0; i < DEVICE_COUNT; ++i) {
                mAudioDevices[i] = false;
            }
        }

        // Store microphone mute state and speakerphone state so it can
        // be restored when closing.
        mSavedIsSpeakerphoneOn = mAudioManager.isSpeakerphoneOn();
        mSavedIsMicrophoneMute = mAudioManager.isMicrophoneMute();

        // Always enable speaker phone by default. This state might be reset
        // by the wired headset receiver when it gets its initial sticky
        // intent, if any.
        setSpeakerphoneOn(true);
        mAudioDeviceState = STATE_SPEAKERPHONE_ON;

        // Initialize audio device list with things we know is always available.
        synchronized (mLock) {
            if (hasEarpiece()) {
                mAudioDevices[DEVICE_EARPIECE] = true;
            }
            mAudioDevices[DEVICE_SPEAKERPHONE] = true;
        }

        // Register receiver for broadcasted intents related to adding/
        // removing a wired headset (Intent.ACTION_HEADSET_PLUG).
        // Also starts routing to the wired headset/headphone if one is
        // already attached (can be overridden by a Bluetooth headset).
        registerForWiredHeadsetIntentBroadcast();

        // Start routing to Bluetooth if there's a connected device.
        // TODO(henrika): the actual routing part is not implemented yet.
        // All we do currently is to detect if BT headset is attached or not.
        initBluetooth();

        mIsInitialized = true;

        mSettingsObserverThread = new SettingsObserverThread();
        mSettingsObserverThread.start();
        synchronized(mSettingsObserverLock) {
            try {
                mSettingsObserverLock.wait();
            } catch (InterruptedException e) {
                Log.e(TAG, "unregisterHeadsetReceiver exception: " + e.getMessage());
            }
        }
    }

    /**
     * Unregister all previously registered intent receivers and restore
     * the stored state (stored in {@link #init()}).
     */
    @CalledByNative
    public void close() {
        if (!mIsInitialized)
            return;

        if (mSettingsObserverThread != null ) {
            mSettingsObserverThread = null;
        }
        if (mSettingsObserver != null) {
            mContentResolver.unregisterContentObserver(mSettingsObserver);
            mSettingsObserver = null;
        }

        unregisterForWiredHeadsetIntentBroadcast();

        // Restore previously stored audio states.
        setMicrophoneMute(mSavedIsMicrophoneMute);
        setSpeakerphoneOn(mSavedIsSpeakerphoneOn);

        mIsInitialized = false;
    }

    @CalledByNative
    public void setMode(int mode) {
        try {
            mAudioManager.setMode(mode);
        } catch (SecurityException e) {
            Log.e(TAG, "setMode exception: " + e.getMessage());
            logDeviceInfo();
        }
    }

    /**
     * Activates, i.e., starts routing audio to, the specified audio device.
     *
     * @param deviceId Unique device ID (integer converted to string)
     * representing the selected device. This string is empty if the so-called
     * default device is selected.
     */
    @CalledByNative
    public void setDevice(String deviceId) {
        boolean devices[] = null;
        synchronized (mLock) {
            devices = mAudioDevices.clone();
        }
        if (deviceId.isEmpty()) {
            logd("setDevice: default");
            // Use a special selection scheme if the default device is selected.
            // The "most unique" device will be selected; Bluetooth first, then
            // wired headset and last the speaker phone.
            if (devices[DEVICE_BLUETOOTH_HEADSET]) {
                // TODO(henrika): possibly need improvements here if we are
                // in a STATE_BLUETOOTH_TURNING_OFF state.
                setAudioDevice(DEVICE_BLUETOOTH_HEADSET);
            } else if (devices[DEVICE_WIRED_HEADSET]) {
                setAudioDevice(DEVICE_WIRED_HEADSET);
            } else {
                setAudioDevice(DEVICE_SPEAKERPHONE);
            }
        } else {
            logd("setDevice: " + deviceId);
            // A non-default device is specified. Verify that it is valid
            // device, and if so, start using it.
            List<Integer> validIds = Arrays.asList(VALID_DEVICES);
            Integer id = Integer.valueOf(deviceId);
            if (validIds.contains(id)) {
                setAudioDevice(id.intValue());
            } else {
                loge("Invalid device ID!");
            }
        }
    }

    /**
     * @return the current list of available audio devices.
     * Note that this call does not trigger any update of the list of devices,
     * it only copies the current state in to the output array.
     */
    @CalledByNative
    public AudioDeviceName[] getAudioInputDeviceNames() {
        synchronized (mLock) {
            List<String> devices = new ArrayList<String>();
            AudioDeviceName[] array = new AudioDeviceName[getNumOfAudioDevicesWithLock()];
            int i = 0;
            for (int id = 0; id < DEVICE_COUNT; ++id ) {
                if (mAudioDevices[id]) {
                    array[i] = new AudioDeviceName(id, DEVICE_NAMES[id]);
                    devices.add(DEVICE_NAMES[id]);
                    i++;
                }
            }
            logd("getAudioInputDeviceNames: " + devices);
            return array;
        }
    }

    @CalledByNative
    private int getNativeOutputSampleRate() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.JELLY_BEAN_MR1) {
            String sampleRateString = mAudioManager.getProperty(
                    AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            return (sampleRateString == null ?
                    DEFAULT_SAMPLING_RATE : Integer.parseInt(sampleRateString));
        } else {
            return DEFAULT_SAMPLING_RATE;
        }
    }

  /**
   * Returns the minimum frame size required for audio input.
   *
   * @param sampleRate sampling rate
   * @param channels number of channels
   */
    @CalledByNative
    private static int getMinInputFrameSize(int sampleRate, int channels) {
        int channelConfig;
        if (channels == 1) {
            channelConfig = AudioFormat.CHANNEL_IN_MONO;
        } else if (channels == 2) {
            channelConfig = AudioFormat.CHANNEL_IN_STEREO;
        } else {
            return -1;
        }
        return AudioRecord.getMinBufferSize(
                sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT) / 2 / channels;
    }

  /**
   * Returns the minimum frame size required for audio output.
   *
   * @param sampleRate sampling rate
   * @param channels number of channels
   */
    @CalledByNative
    private static int getMinOutputFrameSize(int sampleRate, int channels) {
        int channelConfig;
        if (channels == 1) {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        } else if (channels == 2) {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        } else {
            return -1;
        }
        return AudioTrack.getMinBufferSize(
                sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT) / 2 / channels;
    }

    @CalledByNative
    private boolean isAudioLowLatencySupported() {
        return mContext.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_AUDIO_LOW_LATENCY);
    }

    @CalledByNative
    private int getAudioLowLatencyOutputFrameSize() {
        String framesPerBuffer =
                mAudioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        return (framesPerBuffer == null ?
                DEFAULT_FRAME_PER_BUFFER : Integer.parseInt(framesPerBuffer));
    }

    /** Sets the speaker phone mode. */
    public void setSpeakerphoneOn(boolean on) {
        boolean wasOn = mAudioManager.isSpeakerphoneOn();
        if (wasOn == on) {
            return;
        }
        mAudioManager.setSpeakerphoneOn(on);
    }

    /** Sets the microphone mute state. */
    public void setMicrophoneMute(boolean on) {
        boolean wasMuted = mAudioManager.isMicrophoneMute();
        if (wasMuted == on) {
            return;
        }
        mAudioManager.setMicrophoneMute(on);
    }

    /** Gets  the current microphone mute state. */
    public boolean isMicrophoneMute() {
        return mAudioManager.isMicrophoneMute();
    }

    /** Gets the current earpice state. */
    private boolean hasEarpiece() {
        return mContext.getPackageManager().hasSystemFeature(
            PackageManager.FEATURE_TELEPHONY);
    }

    /**
     * Registers receiver for the broadcasted intent when a wired headset is
     * plugged in or unplugged. The received intent will have an extra
     * 'state' value where 0 means unplugged, and 1 means plugged.
     */
    private void registerForWiredHeadsetIntentBroadcast() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);

        /**
         * Receiver which handles changes in wired headset availablilty.
         */
        mWiredHeadsetReceiver = new BroadcastReceiver() {
            private static final int STATE_UNPLUGGED = 0;
            private static final int STATE_PLUGGED = 1;
            private static final int HAS_NO_MIC = 0;
            private static final int HAS_MIC = 1;

            @Override
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                if (!action.equals(Intent.ACTION_HEADSET_PLUG)) {
                    return;
                }
                int state = intent.getIntExtra("state", STATE_UNPLUGGED);
                int microphone = intent.getIntExtra("microphone", HAS_NO_MIC);
                String name = intent.getStringExtra("name");
                logd("==> onReceive: s=" + state
                        + ", m=" + microphone
                        + ", n=" + name
                        + ", sb=" + isInitialStickyBroadcast());

                switch (state) {
                    case STATE_UNPLUGGED:
                        synchronized (mLock) {
                            // Wired headset and earpiece are mutually exclusive.
                            mAudioDevices[DEVICE_WIRED_HEADSET] = false;
                            if (hasEarpiece()) {
                                mAudioDevices[DEVICE_EARPIECE] = true;
                            }
                        }
                        // If wired headset was used before it was unplugged,
                        // switch to speaker phone. If it was not in use; just
                        // log the change.
                        if (mAudioDeviceState == STATE_WIRED_HEADSET_ON) {
                            setAudioDevice(DEVICE_SPEAKERPHONE);
                        } else {
                            reportUpdate();
                        }
                        break;
                    case STATE_PLUGGED:
                        synchronized (mLock) {
                            // Wired headset and earpiece are mutually exclusive.
                            mAudioDevices[DEVICE_WIRED_HEADSET] = true;
                            mAudioDevices[DEVICE_EARPIECE] = false;
                            setAudioDevice(DEVICE_WIRED_HEADSET);
                        }
                        break;
                    default:
                        loge("Invalid state!");
                        break;
                }
            }
        };

        // Note: the intent we register for here is sticky, so it'll tell us
        // immediately what the last action was (plugged or unplugged).
        // It will enable us to set the speakerphone correctly.
        mContext.registerReceiver(mWiredHeadsetReceiver, filter);
    }

    /** Unregister receiver for broadcasted ACTION_HEADSET_PLUG intent. */
    private void unregisterForWiredHeadsetIntentBroadcast() {
        mContext.unregisterReceiver(mWiredHeadsetReceiver);
        mWiredHeadsetReceiver = null;
    }

    /**
    * Check if Bluetooth device is connected, register Bluetooth receiver
    * and start routing to Bluetooth if a device is connected.
    * TODO(henrika): currently only supports the detecion part at startup.
    */
    private void initBluetooth() {
        // Bail out if we don't have the required permission.
        mHasBluetoothPermission = mContext.checkPermission(
            android.Manifest.permission.BLUETOOTH,
            Process.myPid(),
            Process.myUid()) == PackageManager.PERMISSION_GRANTED;
        if (!mHasBluetoothPermission) {
            loge("BLUETOOTH permission is missing!");
            return;
        }

        // To get a BluetoothAdapter representing the local Bluetooth adapter,
        // when running on JELLY_BEAN_MR1 (4.2) and below, call the static
        // getDefaultAdapter() method; when running on JELLY_BEAN_MR2 (4.3) and
        // higher, retrieve it through getSystemService(String) with
        // BLUETOOTH_SERVICE.
        // Note: Most methods require the BLUETOOTH permission.
        BluetoothAdapter btAdapter = null;
        if (android.os.Build.VERSION.SDK_INT <=
            android.os.Build.VERSION_CODES.JELLY_BEAN_MR1) {
            // Use static method for Android 4.2 and below to get the
            // BluetoothAdapter.
            btAdapter = BluetoothAdapter.getDefaultAdapter();
        } else {
            // Use BluetoothManager to get the BluetoothAdapter for
            // Android 4.3 and above.
            BluetoothManager btManager =
                    (BluetoothManager)mContext.getSystemService(
                            Context.BLUETOOTH_SERVICE);
            btAdapter = btManager.getAdapter();
        }

        if (btAdapter != null &&
            // android.bluetooth.BluetoothAdapter.getProfileConnectionState
            // requires BLUETOOTH permission.
            android.bluetooth.BluetoothProfile.STATE_CONNECTED ==
                    btAdapter.getProfileConnectionState(
                            android.bluetooth.BluetoothProfile.HEADSET)) {
            synchronized (mLock) {
                mAudioDevices[DEVICE_BLUETOOTH_HEADSET] = true;
            }
            // TODO(henrika): ensure that we set the active audio
            // device to Bluetooth (not trivial).
            setAudioDevice(DEVICE_BLUETOOTH_HEADSET);
        }
    }

    /**
     * Changes selection of the currently active audio device.
     *
     * @param device Specifies the selected audio device.
     */
    public void setAudioDevice(int device) {
        switch (device) {
            case DEVICE_BLUETOOTH_HEADSET:
                // TODO(henrika): add support for turning on an routing to
                // BT here.
                if (DEBUG) logd("--- TO BE IMPLEMENTED ---");
                break;
            case DEVICE_SPEAKERPHONE:
                // TODO(henrika): turn off BT if required.
                mAudioDeviceState = STATE_SPEAKERPHONE_ON;
                setSpeakerphoneOn(true);
                break;
            case DEVICE_WIRED_HEADSET:
                // TODO(henrika): turn off BT if required.
                mAudioDeviceState = STATE_WIRED_HEADSET_ON;
                setSpeakerphoneOn(false);
                break;
            case DEVICE_EARPIECE:
                // TODO(henrika): turn off BT if required.
                mAudioDeviceState = STATE_EARPIECE_ON;
                setSpeakerphoneOn(false);
                break;
            default:
                loge("Invalid audio device selection!");
                break;
        }
        reportUpdate();
    }

    private int getNumOfAudioDevicesWithLock() {
        int count = 0;
        for (int i = 0; i < DEVICE_COUNT; ++i) {
            if (mAudioDevices[i])
                count++;
        }
        return count;
    }

    /**
     * For now, just log the state change but the idea is that we should
     * notify a registered state change listener (if any) that there has
     * been a change in the state.
     * TODO(henrika): add support for state change listener.
     */
    private void reportUpdate() {
        synchronized (mLock) {
            List<String> devices = new ArrayList<String>();
            for (int i = 0; i < DEVICE_COUNT; ++i) {
                if (mAudioDevices[i])
                    devices.add(DEVICE_NAMES[i]);
            }
            logd("reportUpdate: state=" + mAudioDeviceState
                + ", devices=" + devices);
        }
    }

    private void logDeviceInfo() {
        Log.i(TAG, "Manufacturer:" + Build.MANUFACTURER +
                " Board: " + Build.BOARD + " Device: " + Build.DEVICE +
                " Model: " + Build.MODEL + " PRODUCT: " + Build.PRODUCT);
    }

    /** Trivial helper method for debug logging */
    private void logd(String msg) {
        Log.d(TAG, msg);
    }

    /** Trivial helper method for error logging */
    private void loge(String msg) {
        Log.e(TAG, msg);
    }

    private class SettingsObserver extends ContentObserver {
        SettingsObserver() {
            super(new Handler());
            mContentResolver.registerContentObserver(Settings.System.CONTENT_URI, true, this);
        }

        @Override
        public void onChange(boolean selfChange) {
            super.onChange(selfChange);
            int volume = mAudioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
            nativeSetMute(mNativeAudioManagerAndroid, (volume == 0));
        }
    }

    private native void nativeSetMute(long nativeAudioManagerAndroid, boolean muted);

    private class SettingsObserverThread extends Thread {
        SettingsObserverThread() {
            super("SettinsObserver");
        }

        @Override
        public void run() {
            // Set this thread up so the handler will work on it.
            Looper.prepare();

            synchronized(mSettingsObserverLock) {
                mSettingsObserver = new SettingsObserver();
                mSettingsObserverLock.notify();
            }

            // Listen for volume change.
            Looper.loop();
        }
    }
}
