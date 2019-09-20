// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync.protocol.SharingSpecificFields;
import org.chromium.components.sync.protocol.SyncEnums;

import java.util.ArrayList;

/**
 * JNI bridge for SharingServiceProxy.
 */
public class SharingServiceProxy {
    private static SharingServiceProxy sInstance;

    private static long sNativeSharingServiceProxyAndroid;

    /**
     * @return Singleton instance for this class.
     */
    public static SharingServiceProxy getInstance() {
        if (sInstance != null) {
            return sInstance;
        }

        if (sNativeSharingServiceProxyAndroid == 0) {
            // The service hasn't been created yet.
            Natives jni = SharingServiceProxyJni.get();
            jni.initSharingService(Profile.getLastUsedProfile());
        }

        sInstance = new SharingServiceProxy();
        return sInstance;
    }

    SharingServiceProxy() {}

    @CalledByNative
    private static void onProxyCreated(long nativeSharingServiceProxyAndroid) {
        // There's only one profile in Android, therefore there should only be one service/proxy.
        assert sNativeSharingServiceProxyAndroid == 0;
        sNativeSharingServiceProxyAndroid = nativeSharingServiceProxyAndroid;
    }

    @CalledByNative
    private static void onProxyDestroyed() {
        sNativeSharingServiceProxyAndroid = 0;
    }

    /**
     * Sends a shared clipboard message to the device specified by GUID.
     * @param guid The guid of the device on the receiving end.
     * @param message The text to send.
     * @param callback The result of the operation. Runs |callback| with a
     *         org.chromium.chrome.browser.sharing.SharingSendMessageResult enum value.
     */
    public void sendSharedClipboardMessage(
            String guid, String message, Callback<Integer> callback) {
        if (sNativeSharingServiceProxyAndroid == 0) {
            callback.onResult(SharingSendMessageResult.INTERNAL_ERROR);
            return;
        }

        Natives jni = SharingServiceProxyJni.get();
        jni.sendSharedClipboardMessage(sNativeSharingServiceProxyAndroid, guid, message, callback);
    }

    /**
     * Matches definition in //components/sync_device_info/device_info.h.
     */
    public static class DeviceInfo {
        private DeviceInfo() {}

        public String guid;
        public String clientName;
        public SyncEnums.DeviceType deviceType;
        public long lastUpdatedTimestampMilliseconds;
    }

    @CalledByNative
    private static void createDeviceInfoAndAppendToList(ArrayList<DeviceInfo> deviceInfo,
            String guid, String clientName, int deviceType, long lastUpdatedTimestampMilliseconds) {
        DeviceInfo device = new DeviceInfo();
        device.guid = guid;
        device.clientName = clientName;
        device.deviceType = SyncEnums.DeviceType.valueOf(deviceType);
        device.lastUpdatedTimestampMilliseconds = lastUpdatedTimestampMilliseconds;
        deviceInfo.add(device);
    }

    /**
     * Returns a list of devices for with given capabilities.
     * @param requiredFeature Required feature from SharingSpecificFields.EnabledFeatures enum.
     */
    public ArrayList<DeviceInfo> getDeviceCandidates(
            SharingSpecificFields.EnabledFeatures requiredFeature) {
        ArrayList<DeviceInfo> deviceInfo = new ArrayList<>();
        if (sNativeSharingServiceProxyAndroid == 0) {
            return deviceInfo;
        }

        Natives jni = SharingServiceProxyJni.get();
        jni.getDeviceCandidates(
                sNativeSharingServiceProxyAndroid, deviceInfo, requiredFeature.getNumber());
        return deviceInfo;
    }

    public void addDeviceCandidatesInitializedObserver(Runnable callback) {
        if (sNativeSharingServiceProxyAndroid == 0) {
            callback.run();
            return;
        }

        Natives jni = SharingServiceProxyJni.get();
        jni.addDeviceCandidatesInitializedObserver(sNativeSharingServiceProxyAndroid, callback);
    }

    @NativeMethods
    interface Natives {
        void initSharingService(Profile profile);
        void sendSharedClipboardMessage(long nativeSharingServiceProxyAndroid, String guid,
                String text, Callback<Integer> callback);
        void getDeviceCandidates(long nativeSharingServiceProxyAndroid,
                ArrayList<DeviceInfo> deviceInfo, int requiredFeature);
        void addDeviceCandidatesInitializedObserver(
                long nativeSharingServiceProxyAndroid, Runnable runnable);
    }
}
