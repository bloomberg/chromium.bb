// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;
import android.service.notification.StatusBarNotification;
import android.support.annotation.Nullable;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests for a standalone Web App notification governed by {@link WebappActionsNotificationManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappActionsNotificationTest {
    private static final String WEB_APP_PATH = "/chrome/test/data/banners/manifest_test_page.html";

    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Before
    public void startWebapp() throws Exception {
        mActivityTestRule.startWebappActivity(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_URL, mActivityTestRule.getUrlFromTestServer(WEB_APP_PATH)));
        mActivityTestRule.waitUntilSplashscreenHides();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    @MinAndroidSdkLevel(Build.VERSION_CODES.M) // NotificationManager.getActiveNotifications
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.PWA_PERSISTENT_NOTIFICATION})
    public void testNotification() throws Exception {
        Notification notification = getWebappNotification();

        Assert.assertNotNull(notification);
        Assert.assertEquals("webapp short name runs in Chrome",
                notification.extras.getString(Notification.EXTRA_TITLE));
        Assert.assertEquals(UrlFormatter.formatUrlForDisplay(
                                    mActivityTestRule.getUrlFromTestServer(WEB_APP_PATH)),
                notification.extras.getString(Notification.EXTRA_TEXT));
        Assert.assertEquals("Share", notification.actions[0].title);
        Assert.assertEquals("Open in Chrome", notification.actions[1].title);

        notification.actions[1].actionIntent.send();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ApplicationStatus.getLastTrackedFocusedActivity()
                               instanceof ChromeTabbedActivity;
            }
        });

        Assert.assertNull("Notification should no longer be shown", getWebappNotification());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.M) // NotificationManager.getActiveNotifications
    public void testNotificationNotEnabled() throws Exception {
        // Note ChromeFeatureList.PWA_PERSISTENT_NOTIFICATION is not enabled.
        Assert.assertNull(getWebappNotification());
    }

    @Nullable
    private Notification getWebappNotification() {
        NotificationManager nm =
                (NotificationManager) mActivityTestRule.getActivity().getSystemService(
                        Context.NOTIFICATION_SERVICE);
        for (StatusBarNotification sbn : nm.getActiveNotifications()) {
            if (sbn.getId() == NotificationConstants.NOTIFICATION_ID_WEBAPP_ACTIONS) {
                return sbn.getNotification();
            }
        }
        return null;
    }
}
