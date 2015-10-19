// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.SpannableStringBuilder;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;

/**
 * Instrumentation unit tests for CustomNotificationBuilder.
 */
public class CustomNotificationBuilderTest extends InstrumentationTestCase {
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

        Notification notification = new CustomNotificationBuilder(context)
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
                                            .build();
        View view = notification.contentView.apply(context, new LinearLayout(context));

        assertEquals(R.drawable.ic_chrome, notification.icon);
        assertNotNull(((ImageView) view.findViewById(R.id.icon)).getDrawable());
        assertEquals("title", ((TextView) view.findViewById(R.id.title)).getText());
        assertEquals("body", ((TextView) view.findViewById(R.id.body)).getText());
        assertEquals("origin", ((TextView) view.findViewById(R.id.origin)).getText());
        assertEquals("ticker", notification.tickerText.toString());
        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(1, notification.vibrate.length);
        assertEquals(100L, notification.vibrate[0]);
        assertEquals(pendingContentIntent, notification.contentIntent);
        assertEquals(pendingDeleteIntent, notification.deleteIntent);
        // TODO(mvanouwerkerk): Add coverage for action buttons.
    }
}
