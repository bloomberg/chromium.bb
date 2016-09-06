// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.os.Build;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.SpannableStringBuilder;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;

/**
 * Instrumentation unit tests for StandardNotificationBuilder.
 */
public class StandardNotificationBuilderTest extends InstrumentationTestCase {
    private NotificationBuilderBase createAllOptionsBuilder(
            PendingIntent[] outContentAndDeleteIntents) {
        if (outContentAndDeleteIntents == null || outContentAndDeleteIntents.length != 2)
            throw new IllegalArgumentException();

        Context context = getInstrumentation().getTargetContext();

        Intent contentIntent = new Intent("contentIntent");
        outContentAndDeleteIntents[0] = PendingIntent.getBroadcast(
                context, 0 /* requestCode */, contentIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        Intent deleteIntent = new Intent("deleteIntent");
        outContentAndDeleteIntents[1] = PendingIntent.getBroadcast(
                context, 1 /* requestCode */, deleteIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        Bitmap image = Bitmap.createBitmap(
                new int[] {Color.BLUE}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        image = image.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);

        Bitmap largeIcon = Bitmap.createBitmap(
                new int[] {Color.RED}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        largeIcon = largeIcon.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);

        Bitmap actionIcon = Bitmap.createBitmap(
                new int[] {Color.GRAY}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        actionIcon = actionIcon.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);

        return new StandardNotificationBuilder(context)
                .setTitle("title")
                .setBody("body")
                .setOrigin("origin")
                .setTicker(new SpannableStringBuilder("ticker"))
                .setImage(image)
                .setLargeIcon(largeIcon)
                .setSmallIcon(R.drawable.ic_chrome)
                .setDefaults(Notification.DEFAULT_ALL)
                .setVibrate(new long[] {100L})
                .setContentIntent(outContentAndDeleteIntents[0])
                .setDeleteIntent(outContentAndDeleteIntents[1])
                .addAction(actionIcon, "button 1", null /* intent */)
                .addAction(actionIcon, "button 2", null /* intent */)
                .addSettingsAction(0 /* iconId */, "settings", null /* intent */);
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testSetAll() {
        PendingIntent[] contentAndDeleteIntents = new PendingIntent[2];
        NotificationBuilderBase builder = createAllOptionsBuilder(contentAndDeleteIntents);
        Notification notification = builder.build();

        assertEquals("title", NotificationTestUtil.getExtraTitle(notification));
        assertEquals("body", NotificationTestUtil.getExtraText(notification));
        assertEquals("origin", NotificationTestUtil.getExtraSubText(notification));
        assertEquals("ticker", notification.tickerText.toString());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // EXTRA_TEMPLATE was added in Android L; style cannot be verified in earlier versions.
            assertEquals("android.app.Notification$BigPictureStyle",
                    notification.extras.getString(Notification.EXTRA_TEMPLATE));
        }
        Bitmap picture = NotificationTestUtil.getExtraPicture(notification);
        assertNotNull(picture);
        assertTrue(picture.getWidth() > 0 && picture.getHeight() > 0);

        Context context = getInstrumentation().getTargetContext();
        Bitmap smallIcon =
                BitmapFactory.decodeResource(context.getResources(), R.drawable.ic_chrome);
        assertTrue(smallIcon.sameAs(
                NotificationTestUtil.getSmallIconFromNotification(context, notification)));
        assertNotNull(NotificationTestUtil.getLargeIconFromNotification(context, notification));

        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(1, notification.vibrate.length);
        assertEquals(100L, notification.vibrate[0]);
        assertEquals(contentAndDeleteIntents[0], notification.contentIntent);
        assertEquals(contentAndDeleteIntents[1], notification.deleteIntent);
        Notification.Action[] actions = NotificationTestUtil.getActions(notification);
        assertEquals(3, actions.length);
        assertEquals("button 1", NotificationTestUtil.getActionTitle(actions[0]));
        assertEquals("button 2", NotificationTestUtil.getActionTitle(actions[1]));
        assertEquals("settings", NotificationTestUtil.getActionTitle(actions[2]));
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testBigTextStyle() {
        PendingIntent[] contentAndDeleteIntents = new PendingIntent[2];
        NotificationBuilderBase builder = createAllOptionsBuilder(contentAndDeleteIntents);
        builder.setImage(null);
        Notification notification = builder.build();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // EXTRA_TEMPLATE was added in Android L; style cannot be verified in earlier versions.
            assertEquals("android.app.Notification$BigTextStyle",
                    notification.extras.getString(Notification.EXTRA_TEMPLATE));
        }
    }
}
