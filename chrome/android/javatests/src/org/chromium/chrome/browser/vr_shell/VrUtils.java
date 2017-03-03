// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;

import com.google.protobuf.nano.MessageNano;
import com.google.vr.vrcore.proto.nano.Nfc.NfcParams;

import org.chromium.base.ThreadUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.Callable;

/**
 * Class containing static functions that are useful for VR instrumentation
 * testing.
 */
public class VrUtils {
    private static final String DETECTION_ACTIVITY =
            ".nfc.ViewerDetectionActivity";
    // TODO(bsheedy): Use constants from VrCore if ever exposed
    private static final String APPLICATION_RECORD_STRING = "com.google.vr.vrcore";
    private static final String RESERVED_KEY = "google.vr/rsvd";
    private static final String VERSION_KEY =  "google.vr/ver";
    private static final String DATA_KEY =     "google.vr/data";
    private static final ByteOrder BYTE_ORDER = ByteOrder.BIG_ENDIAN;
    private static final int VIEWER_ID = 3;
    private static final int VERSION = 123;
    private static final int RESERVED = 456;

    /**
     * Forces the browser into VR mode via a VrShellDelegate call.
     */
    public static void forceEnterVr() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                VrShellDelegate.enterVRIfNecessary();
            }
        });
    }

    /**
     * Forces the browser out of VR mode via a VrShellDelegate call.
     * @param vrDelegate The VRShellDelegate associated with this activity.
     */
    public static void forceExitVr(final VrShellDelegate vrDelegate) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                vrDelegate.shutdownVR(false /* isPausing */, false /* showTransition */);
            }
        });
    }

    private static byte[] intToByteArray(int i) {
        final ByteBuffer bb = ByteBuffer.allocate(Integer.SIZE / Byte.SIZE);
        bb.order(BYTE_ORDER);
        bb.putInt(i);
        return bb.array();
    }

    /**
     * Makes an Intent that triggers VrCore as if the Daydream headset's NFC
     * tag was scanned.
     * @return The intent to send to VrCore to simulate an NFC scan.
     */
    public static Intent makeNfcIntent() {
        Intent nfcIntent = new Intent(NfcAdapter.ACTION_NDEF_DISCOVERED);
        NfcParams nfcParams = new NfcParams();
        nfcParams.setViewerId(VIEWER_ID);
        byte[] protoBytes = MessageNano.toByteArray(nfcParams);
        NdefMessage[] messages = new NdefMessage[] {
                new NdefMessage(new NdefRecord[] {
                        NdefRecord.createMime(
                                RESERVED_KEY, intToByteArray(RESERVED)),
                        NdefRecord.createMime(
                                VERSION_KEY, intToByteArray(VERSION)),
                        NdefRecord.createMime(DATA_KEY, protoBytes),
                        NdefRecord.createApplicationRecord(
                                APPLICATION_RECORD_STRING)})};
        nfcIntent.putExtra(NfcAdapter.EXTRA_NDEF_MESSAGES, messages);
        nfcIntent.setComponent(new ComponentName(APPLICATION_RECORD_STRING,
                  APPLICATION_RECORD_STRING + ".nfc.ViewerDetectionActivity"));
        return nfcIntent;
    }

    /**
     * Simulates the NFC tag of the Daydream headset being scanned.
     * @param context The Context that the activity will be started from.
     */
    public static void simNfc(Context context) {
        Intent nfcIntent = makeNfcIntent();
        try {
            context.startActivity(nfcIntent);
        } catch (ActivityNotFoundException e) {
            // On unsupported devices, won't find VrCore -> Do nothing
        }
    }

    /**
     * Waits until the given VrShellDelegate's isInVR() returns true. Should
     * only be used when VR Shell support is expected.
     */
    public static void waitForVrSupported() {
        // If VR Shell is supported, mInVr should eventually go to true
        // Relatively long timeout because sometimes GVR takes a while to enter VR
        CriteriaHelper.pollUiThread(Criteria.equals(true, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return VrShellDelegate.isInVR();
            }
        }), 10000, 50);
    }
}
