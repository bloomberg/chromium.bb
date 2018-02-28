// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

import java.io.File;
import java.io.IOException;

/**
 * AwVariationsSeedHandler is used to received the seed data from the
 * AwVariationsConfigurationService on the WebView side. It will copy both the seed data and
 * preference from the service directory into the app directory.
 */
public class AwVariationsSeedHandler extends Handler {
    private static final String TAG = "AwVariatnsWVHdlr";

    private ServiceConnection mConnection;

    AwVariationsSeedHandler(ServiceConnection connection) {
        mConnection = connection;
    }

    /**
     * Method called when receiving a response from the VariationsConfigurationService, a response
     * will be recieved even in the event there is no seed data.
     */
    @Override
    public void handleMessage(Message msg) {
        if (msg.what == AwVariationsUtils.MSG_WITH_SEED_DATA) {
            Bundle data = msg.getData();
            ParcelFileDescriptor seedDataFileDescriptor =
                    data.getParcelable(AwVariationsUtils.KEY_SEED_DATA);
            ParcelFileDescriptor seedPrefFileDescriptor =
                    data.getParcelable(AwVariationsUtils.KEY_SEED_PREF);

            try {
                File variationsDir = getOrCreateVariationsDirectory();
                AwVariationsUtils.copyFileToVariationsDirectory(seedDataFileDescriptor,
                        variationsDir, AwVariationsUtils.SEED_DATA_FILENAME);
                AwVariationsUtils.copyFileToVariationsDirectory(seedPrefFileDescriptor,
                        variationsDir, AwVariationsUtils.SEED_PREF_FILENAME);
            } catch (IOException e) {
                Log.e(TAG, "Failed to copy seed from seed directory to app directory. " + e);
            }
        }

        // Unbind from the AwVariationsConfigurationService.
        ContextUtils.getApplicationContext().unbindService(mConnection);
    }

    /**
     * Bind to AwVariationsConfigurationService in the current context to ask for a seed.
     */
    public static void bindToVariationsService(String webViewPackageName) {
        File appWebViewVariationsDir = null;
        try {
            appWebViewVariationsDir = getOrCreateVariationsDirectory();
        } catch (IOException e) {
            Log.e(TAG, "Failed to create the WebView Variations directory. " + e);
            return;
        }

        AwVariationsUtils.SeedPreference seedPref = null;
        try {
            seedPref = AwVariationsUtils.readSeedPreference(appWebViewVariationsDir);
        } catch (IOException e) {
            Log.e(TAG, "Failed to read seed preference file. " + e);
            return;
        }

        if (!seedPref.seedNeedsUpdate()) {
            Log.d(TAG,
                    "The current variations seed is not expired, no need to ask for a new seed.");
            return;
        }

        ServiceConnection connection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                Message msg = Message.obtain();
                msg.what = AwVariationsUtils.MSG_SEED_REQUEST;
                msg.replyTo = new Messenger(new AwVariationsSeedHandler(this));
                try {
                    // Send a message to request for a new seed
                    new Messenger(service).send(msg);
                } catch (RemoteException e) {
                    Log.e(TAG, "Failed to seed message to AwVariationsConfigurationService. " + e);
                }
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };

        Intent intent = new Intent();
        intent.setClassName(webViewPackageName, AwVariationsConfigurationService.class.getName());
        Context context = ContextUtils.getApplicationContext();
        if (!context.bindService(intent, connection, Context.BIND_AUTO_CREATE)) {
            Log.w(TAG, "Could not bind to AwVariationsConfigurationService. " + intent);
        }
    }

    /**
     * Get or create the variations directory in the service directory.
     * @return The variations directory path.
     * @throws IOException if fail to create the directory and get the directory.
     */
    @VisibleForTesting
    public static File getOrCreateVariationsDirectory() throws IOException {
        File dir = new File(PathUtils.getDataDirectory());

        if (dir.mkdir() || dir.isDirectory()) {
            return dir;
        }
        throw new IOException("Failed to get or create the WebView variations directory.");
    }

    /**
     * Clear the test data.
     * @throws IOException if fail to get or create the WebView variations directory.
     */
    @VisibleForTesting
    public static void clearDataForTesting() throws IOException {
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        File variationsDir = getOrCreateVariationsDirectory();
        FileUtils.recursivelyDeleteFile(variationsDir);
    }
}
