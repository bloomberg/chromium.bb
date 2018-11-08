// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static android.app.PendingIntent.FLAG_UPDATE_CURRENT;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;
import static org.chromium.chrome.browser.notifications.NotificationConstants.NOTIFICATION_ID_TWA_PERSISTENT;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Publishes and dismisses the notification when running Trusted Web Activities. The notification
 * offers to manage site data and to share info about it.
 *
 * The notification is shown while the activity is in started state. It is not removed when the user
 * leaves the origin associated with the app by following links.
 */
@ActivityScope
public class PersistentNotificationController implements StartStopWithNativeObserver, Destroyable {
    private final Context mAppContext;
    private final CustomTabIntentDataProvider mIntentDataProvider;

    @Nullable
    private String mVerifiedPackage;
    @Nullable
    private Origin mVerifiedOrigin;
    private boolean mStarted;

    @Nullable
    private Handler mHandler;

    @Inject
    public PersistentNotificationController(@Named(APP_CONTEXT) Context context,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabIntentDataProvider intentDataProvider) {
        mAppContext = context;
        mIntentDataProvider = intentDataProvider;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onStartWithNative() {
        mStarted = true;
        if (mVerifiedPackage != null) {
            publish();
        }
    }

    @Override
    public void onStopWithNative() {
        mStarted = false;
        dismiss();
    }

    @Override
    public void destroy() {
        killBackgroundThread();
    }

    /**
     * Called when the relationship between an origin and an app with given package name has been
     * verified.
     */
    public void onOriginVerifiedForPackage(Origin origin, String packageName) {
        if (packageName.equals(mVerifiedPackage)) {
            return;
        }
        mVerifiedPackage = packageName;
        mVerifiedOrigin = origin;
        if (mStarted) {
            publish();
        }
    }

    private void publish() {
        postToBackgroundThread(new PublishTask(
                mVerifiedPackage, mVerifiedOrigin, mAppContext, mIntentDataProvider.getIntent()));
    }

    private void dismiss() {
        postToBackgroundThread(new DismissTask(mAppContext, mVerifiedPackage));
    }

    private void postToBackgroundThread(Runnable task) {
        if (mHandler == null) {
            HandlerThread backgroundThread = new HandlerThread("TwaPersistentNotification");
            backgroundThread.start();
            mHandler = new Handler(backgroundThread.getLooper());
        }
        mHandler.post(task);
    }

    private void killBackgroundThread() {
        if (mHandler == null) {
            return;
        }
        Looper looper = mHandler.getLooper();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            looper.quitSafely();
        } else {
            looper.quit();
        }
    }

    private static class PublishTask implements Runnable {
        private final String mPackageName;
        private final Origin mOrigin;
        private final Context mAppContext;
        private final Intent mCustomTabsIntent;

        private PublishTask(String packageName, Origin origin, Context appContext,
                Intent customTabsIntent) {
            mPackageName = packageName;
            mOrigin = origin;
            mAppContext = appContext;
            mCustomTabsIntent = customTabsIntent;
        }

        @Override
        public void run() {
            Notification notification = createNotification();
            NotificationManager nm = (NotificationManager) mAppContext.getSystemService(
                    Context.NOTIFICATION_SERVICE);
            if (nm != null) {
                nm.notify(mPackageName, NOTIFICATION_ID_TWA_PERSISTENT, notification);
            }
        }

        private Notification createNotification() {
            return NotificationBuilderFactory
                    .createChromeNotificationBuilder(true /* preferCompat */,
                            ChannelDefinitions.ChannelId.BROWSER)
                    .setSmallIcon(R.drawable.ic_chrome)
                    .setContentTitle(makeTitle())
                    .setContentText(
                            mAppContext.getString(R.string.app_running_in_chrome_disclosure))
                    .setAutoCancel(false)
                    .setOngoing(true)
                    .setPriorityBeforeO(NotificationCompat.PRIORITY_LOW)
                    .addAction(0 /* icon */, // TODO(pshmakov): set the icons.
                            mAppContext.getString(R.string.share),
                            makeShareIntent())
                    .addAction(0 /* icon */,
                            mAppContext.getString(R.string.twa_manage_data),
                            makeManageDataIntent())
                    .build();
        }

        private String makeTitle() {
            PackageManager packageManager = mAppContext.getPackageManager();
            try {
                return packageManager
                        .getApplicationLabel(packageManager.getApplicationInfo(mPackageName, 0))
                        .toString();
            } catch (PackageManager.NameNotFoundException e) {
                assert false : mPackageName + " not found";
                return "";
            }
        }

        private PendingIntent makeManageDataIntent() {
            Intent settingsIntent = PreferencesLauncher.createIntentForSettingsPage(mAppContext,
                    SingleWebsitePreferences.class.getName(),
                    SingleWebsitePreferences.createFragmentArgsForSite(mOrigin.toString()));
            return PendingIntent.getActivity(mAppContext, 0, settingsIntent, FLAG_UPDATE_CURRENT);
        }

        private PendingIntent makeShareIntent() {
            Intent shareIntent =
                    BrowserSessionContentUtils.createShareIntent(mAppContext, mCustomTabsIntent);
            return PendingIntent.getActivity(mAppContext, 0, shareIntent, FLAG_UPDATE_CURRENT);
        }
    }

    private static class DismissTask implements Runnable {
        private final Context mAppContext;
        private final String mPackageName;

        private DismissTask(Context appContext, String packageName) {
            mAppContext = appContext;
            mPackageName = packageName;
        }

        @Override
        public void run() {
            NotificationManager nm = (NotificationManager) mAppContext.getSystemService(
                    Context.NOTIFICATION_SERVICE);
            if (nm != null) {
                nm.cancel(mPackageName, NOTIFICATION_ID_TWA_PERSISTENT);
            }
        }
    }
}
