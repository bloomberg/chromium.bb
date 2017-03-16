// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.crash;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.webkit.ValueCallback;

import org.chromium.android_webview.PlatformServiceBridge;
import org.chromium.android_webview.command_line.CommandLineUtil;
import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.minidump_uploader.MinidumpUploaderDelegate;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;

/**
 * Android Webview-specific implementations for minidump uploading logic.
 */
public class AwMinidumpUploaderDelegate implements MinidumpUploaderDelegate {
    private final Context mContext;
    private final ConnectivityManager mConnectivityManager;

    private boolean mPermittedByUser = false;

    @VisibleForTesting
    public AwMinidumpUploaderDelegate(Context context) {
        mContext = context;
        mConnectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    @Override
    public File getCrashParentDir() {
        return CrashReceiverService.createWebViewCrashDir(mContext);
    }

    @Override
    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
        return new CrashReportingPermissionManager() {
            @Override
            public boolean isClientInMetricsSample() {
                // We will check whether the client is in the metrics sample before
                // generating a minidump - so if no minidump is generated this code will
                // never run and we don't need to check whether we are in the sample.
                // TODO(gsennton): when we switch to using Finch for this value we should use the
                // Finch value here as well.
                return true;
            }
            @Override
            public boolean isNetworkAvailableForCrashUploads() {
                // JobScheduler will call onStopJob causing our upload to be interrupted when our
                // network requirements no longer hold.
                // TODO(isherman): This code should really be shared with Chrome. Chrome currently
                // checks only whether the network is WiFi (or ethernet) vs. cellular. Most likely,
                // Chrome should instead check whether the network is metered, as is done here.
                NetworkInfo networkInfo = mConnectivityManager.getActiveNetworkInfo();
                if (networkInfo == null || !networkInfo.isConnected()) return false;
                return !mConnectivityManager.isActiveNetworkMetered();
            }
            @Override
            public boolean isCrashUploadDisabledByCommandLine() {
                return false;
            }
            /**
             * This method is already represented by isClientInMetricsSample() and
             * isNetworkAvailableForCrashUploads().
             */
            @Override
            public boolean isMetricsUploadPermitted() {
                return true;
            }
            @Override
            public boolean isUsageAndCrashReportingPermittedByUser() {
                return mPermittedByUser;
            }
            @Override
            public boolean isUploadEnabledForTests() {
                // Note that CommandLine/CommandLineUtil are not thread safe. They are initialized
                // on the main thread, but before the current worker thread started - so this thread
                // will have seen the initialization of the CommandLine.
                return CommandLine.getInstance().hasSwitch(
                        CommandLineUtil.CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH);
            }
        };
    }

    @Override
    public void prepareToUploadMinidumps(final Runnable startUploads) {
        createPlatformServiceBridge().queryMetricsSetting(new ValueCallback<Boolean>() {
            public void onReceiveValue(Boolean enabled) {
                ThreadUtils.assertOnUiThread();
                mPermittedByUser = enabled;
                startUploads.run();
            }
        });
    }

    @Override
    public void recordUploadSuccess(File minidump) {}

    @Override
    public void recordUploadFailure(File minidump) {}

    /**
     * Utility method to allow us to test the logic of this class by injecting
     * a test-specific PlatformServiceBridge.
     */
    @VisibleForTesting
    public PlatformServiceBridge createPlatformServiceBridge() {
        return PlatformServiceBridge.getInstance(mContext);
    }
}
