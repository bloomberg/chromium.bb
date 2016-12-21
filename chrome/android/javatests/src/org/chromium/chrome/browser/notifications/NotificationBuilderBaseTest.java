// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Instrumentation unit tests for NotificationBuilderBase.
 *
 * Extends NativeLibraryTestBase so that {@link UrlUtilities#getDomainAndRegistry} can access
 * native GetDomainAndRegistry, when called by {@link RoundedIconGenerator#getIconTextForUrl} during
 * testEnsureNormalizedIconBehavior().
 */
public class NotificationBuilderBaseTest extends NativeLibraryTestBase {
    @Override
    public void setUp() throws Exception {
        super.setUp();
        // Not initializing the browser process is safe because GetDomainAndRegistry() is
        // stand-alone.
        loadNativeLibraryNoBrowserProcess();
    }

    /**
     * Tests the three paths for ensuring that a notification will be shown with a normalized icon:
     *     (1) NULL bitmaps should have an auto-generated image.
     *     (2) Large bitmaps should be resized to the device's intended size.
     *     (3) Smaller bitmaps should be left alone.
     */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testEnsureNormalizedIconBehavior() throws Exception {
        // Get the dimensions of the notification icon that will be presented to the user.
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        Resources resources = appContext.getResources();

        int largeIconWidthPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);
        int largeIconHeightPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);

        String origin = "https://example.com";

        NotificationBuilderBase notificationBuilder = new NotificationBuilderBase(resources) {
            @Override
            public Notification build() {
                return null;
            }
        };
        Bitmap fromNullIcon = notificationBuilder.ensureNormalizedIcon(null, origin);
        assertNotNull(fromNullIcon);
        assertEquals(largeIconWidthPx, fromNullIcon.getWidth());
        assertEquals(largeIconHeightPx, fromNullIcon.getHeight());

        Bitmap largeIcon = Bitmap.createBitmap(
                largeIconWidthPx * 2, largeIconHeightPx * 2, Bitmap.Config.ALPHA_8);

        Bitmap fromLargeIcon = notificationBuilder.ensureNormalizedIcon(largeIcon, origin);
        assertNotNull(fromLargeIcon);
        assertEquals(largeIconWidthPx, fromLargeIcon.getWidth());
        assertEquals(largeIconHeightPx, fromLargeIcon.getHeight());

        Bitmap smallIcon = Bitmap.createBitmap(
                largeIconWidthPx / 2, largeIconHeightPx / 2, Bitmap.Config.ALPHA_8);

        Bitmap fromSmallIcon = notificationBuilder.ensureNormalizedIcon(smallIcon, origin);
        assertNotNull(fromSmallIcon);
        assertEquals(smallIcon, fromSmallIcon);
    }
}
