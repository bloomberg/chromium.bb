// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import static android.app.NotificationManager.IMPORTANCE_NONE;

import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.TWA_DISCLOSURE_INITIAL;
import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.TWA_DISCLOSURE_SUBSEQUENT;

import android.annotation.TargetApi;
import android.app.NotificationChannel;
import android.os.Build;

import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.TwaDisclosureUi;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureNotification;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureSnackbar;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.DisclosureInfobar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Determines which of the versions of the "Running in Chrome" UI is displayed to the user.
 *
 * There are three:
 * * The old Infobar. (An Infobar doesn't go away until you accept it.)
 * * The new Notification. (When notifications are enabled.)
 * * The new Snackbar. (A Snackbar dismisses automatically, this one after 7 seconds.)
 */
@ActivityScope
public class DisclosureUiPicker implements NativeInitObserver {
    private final Lazy<DisclosureInfobar> mDisclosureInfobar;
    private final Lazy<DisclosureSnackbar> mDisclosureSnackbar;
    private final Lazy<DisclosureNotification> mDisclosureNotification;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final NotificationManagerProxy mNotificationManager;

    @Inject
    public DisclosureUiPicker(
            Lazy<DisclosureInfobar> disclosureInfobar,
            Lazy<DisclosureSnackbar> disclosureSnackbar,
            Lazy<DisclosureNotification> disclosureNotification,
            BrowserServicesIntentDataProvider intentDataProvider,
            NotificationManagerProxy notificationManager,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mDisclosureInfobar = disclosureInfobar;
        mDisclosureSnackbar = disclosureSnackbar;
        mDisclosureNotification = disclosureNotification;
        mIntentDataProvider = intentDataProvider;
        mNotificationManager = notificationManager;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        // Calling get on the appropriate Lazy instance will cause Dagger to create the class.
        // The classes wire themselves up to the rest of the code in their constructors.

        // TODO(peconn): Once this feature is enabled by default and we get rid of this check, move
        // this logic into the constructor and let the Views call showIfNeeded() themselves in
        // their onFinishNativeInitialization.

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NEW_DISCLOSURE)
                || mIntentDataProvider.getTwaDisclosureUi() == TwaDisclosureUi.V1_INFOBAR) {
            mDisclosureInfobar.get().showIfNeeded();
        } else if (areNotificationsEnabled()) {
            mDisclosureNotification.get().onStartWithNative();
        } else {
            mDisclosureSnackbar.get().showIfNeeded();
        }
    }

    private boolean areNotificationsEnabled() {
        if (!mNotificationManager.areNotificationsEnabled()) return false;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return true;

        return isChannelEnabled(TWA_DISCLOSURE_INITIAL)
                && isChannelEnabled(TWA_DISCLOSURE_SUBSEQUENT);
    }

    @TargetApi(Build.VERSION_CODES.O)
    private boolean isChannelEnabled(String channelId) {
        NotificationChannel channel = mNotificationManager.getNotificationChannel(channelId);

        // If the Channel is null we've not created it yet. Since we know that Chrome notifications
        // are not disabled in general, we know that once the channel is created it should be
        // enabled.
        if (channel == null) return true;

        return channel.getImportance() != IMPORTANCE_NONE;
    }
}
