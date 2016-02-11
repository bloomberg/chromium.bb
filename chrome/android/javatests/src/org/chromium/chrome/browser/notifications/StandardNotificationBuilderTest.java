// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.SpannableStringBuilder;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;

/**
 * Instrumentation unit tests for StandardNotificationBuilder.
 */
// TODO(peter): remove @SuppressLint once crbug.com/501900 is fixed.
@SuppressLint("NewApi")
@SuppressWarnings("deprecation") // For |icon| and |largeIcon| properties of Notification.
public class StandardNotificationBuilderTest extends InstrumentationTestCase {
    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testSetAll() {
        Context context = getInstrumentation().getTargetContext();

        Intent contentIntent = new Intent("contentIntent");
        PendingIntent pendingContentIntent = PendingIntent.getBroadcast(
                context, 0 /* requestCode */, contentIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        Intent deleteIntent = new Intent("deleteIntent");
        PendingIntent pendingDeleteIntent = PendingIntent.getBroadcast(
                context, 1 /* requestCode */, deleteIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        Bitmap largeIcon = Bitmap.createBitmap(
                new int[] {Color.RED}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);

        Bitmap actionIcon = Bitmap.createBitmap(
                new int[] {Color.GRAY}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);

        Notification notification =
                new StandardNotificationBuilder(context)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setLargeIcon(largeIcon)
                        .setTitle("title")
                        .setBody("body")
                        .setOrigin("origin")
                        .setTicker(new SpannableStringBuilder("ticker"))
                        .setDefaults(Notification.DEFAULT_ALL)
                        .setVibrate(new long[] {100L})
                        .setContentIntent(pendingContentIntent)
                        .setDeleteIntent(pendingDeleteIntent)
                        .addAction(actionIcon, "button 1", null /* intent */)
                        .addAction(actionIcon, "button 2", null /* intent */)
                        .addSettingsAction(0 /* iconId */, "settings", null /* intent */)
                        .build();

        assertEquals(R.drawable.ic_chrome, notification.icon);
        assertNotNull(notification.largeIcon);
        assertEquals("title", notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals("body", notification.extras.getString(Notification.EXTRA_TEXT));
        assertEquals("origin", notification.extras.getString(Notification.EXTRA_SUB_TEXT));
        assertEquals("ticker", notification.tickerText.toString());
        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(1, notification.vibrate.length);
        assertEquals(100L, notification.vibrate[0]);
        assertEquals(pendingContentIntent, notification.contentIntent);
        assertEquals(pendingDeleteIntent, notification.deleteIntent);
        assertEquals(3, notification.actions.length);
        assertEquals("button 1", notification.actions[0].title);
        assertEquals("button 2", notification.actions[1].title);
        assertEquals("settings", notification.actions[2].title);
    }
}
