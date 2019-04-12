// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.support.annotation.WorkerThread;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.browserservices.BrowserServicesMetrics;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import javax.inject.Inject;
import javax.inject.Named;
import javax.inject.Singleton;

/**
 * This class updates the notification permission for an Origin based on the notification permission
 * that the linked TWA has in Android. It also reverts the notification permission back to that the
 * Origin had before a TWA was installed in the case of TWA uninstallation.
 *
 * TODO(peconn): Add a README.md for Notification Delegation.
 * TODO(peconn): Revert the permission when the TWA is uninstalled.
 * TODO(peconn): Update the permission when a push notification occurs.
 */
@Singleton
public class NotificationPermissionUpdater {
    private static final String TAG = "TWANotifications";

    private final TrustedWebActivityPermissionManager mPermissionManager;
    private final PackageManager mPackageManager;
    private final TrustedWebActivityClient mTrustedWebActivityClient;

    @Inject
    public NotificationPermissionUpdater(@Named(APP_CONTEXT) Context context,
            TrustedWebActivityPermissionManager permissionManager,
            TrustedWebActivityClient trustedWebActivityClient) {
        mPackageManager = context.getPackageManager();
        mPermissionManager = permissionManager;
        mTrustedWebActivityClient = trustedWebActivityClient;
    }

    /**
     * To be called when an origin is verified with a package. It sets the notification permission
     * for that origin according to the following:
     * - If the package doesn't handle browsable intents for the origin, it does nothing.
     * - If the package does handle browsable intents for the origin, but there is no
     *   TrustedWebActivityService (on the entire system), it will *block* notifications for that
     *   origin.
     * - If the package handles browsable intents for the origin and a TrustedwebActivityService
     *   is found, it updates Chrome's notification permission for that origin to match Android's
     *   notification permission for the package.
     */
    public void onOriginVerified(Origin origin, String packageName) {
        // If the client doesn't handle browsable Intents for the URL, we don't do anything special
        // for the origin's notifications.
        if (!appHandlesBrowsableIntent(packageName, origin.uri())) {
            Log.d(TAG, "Package does not handle Browsable Intents for the origin");
            return;
        }

        // It's important to note here that the client we connect to to check for the notification
        // permission may not be the client that triggered this method call.
        boolean couldConnect = mTrustedWebActivityClient.checkNotificationPermission(origin,
                enabled -> updatePermission(origin, enabled));

        if (!couldConnect) {
            Log.w(TAG, "No TWAService found for origin, disabling notifications");
            mPermissionManager.register(origin, false);
        }
    }

    /**
     * If the uninstalled client app results in there being no more TrustedWebActivityService for
     * the origin, return the origin's notification permission to what it was before any client
     * app was installed.
     */
    public void onClientAppUninstalled(Origin origin) {
        // See if there is any other app installed that could handle the notifications (and update
        // to that apps notification permission if it exists).
        boolean couldConnect = mTrustedWebActivityClient.checkNotificationPermission(origin,
                enabled -> updatePermission(origin, enabled));

        // If not, we return notification state to what it was before installation.
        if (!couldConnect) {
            mPermissionManager.unregister(origin);
        }
    }

    @WorkerThread
    private void updatePermission(Origin origin, boolean enabled) {
        // This method will be called by the TrustedWebActivityClient on a background thread, so
        // hop back over to the UI thread to deal with the result.
        PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> {
            mPermissionManager.register(origin, enabled);
            Log.d(TAG, "Updating origin notification permissions to: %b", enabled);
        });
    }

    private boolean appHandlesBrowsableIntent(String packageName, Uri uri) {
        Intent browsableIntent = new Intent();
        browsableIntent.setPackage(packageName);
        browsableIntent.setData(uri);
        browsableIntent.setAction(Intent.ACTION_VIEW);
        browsableIntent.addCategory(Intent.CATEGORY_BROWSABLE);

        try (BrowserServicesMetrics.TimingMetric unused =
                     BrowserServicesMetrics.getBrowsableIntentResolutionTimingContext()) {
            return mPackageManager.resolveActivity(browsableIntent, 0) != null;
        }
    }
}
