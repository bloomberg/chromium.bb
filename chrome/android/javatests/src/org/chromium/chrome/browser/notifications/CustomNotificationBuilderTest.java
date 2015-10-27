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

import java.util.ArrayList;

/**
 * Instrumentation unit tests for CustomNotificationBuilder.
 */
@SuppressWarnings("deprecation") // For the |icon| and |largeIcon| properties of Notification.
public class CustomNotificationBuilderTest extends InstrumentationTestCase {
    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testSetAll() {
        Context context = getInstrumentation().getTargetContext();

        PendingIntent contentIntent = createIntent(context, "Content");
        PendingIntent deleteIntent = createIntent(context, "Delete");

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
                                            .setContentIntent(contentIntent)
                                            .setDeleteIntent(deleteIntent)
                                            .addAction(0 /* iconId */, "button",
                                                    createIntent(context, "ActionButtonOne"))
                                            .addAction(0 /* iconId */, "button",
                                                    createIntent(context, "ActionButtonTwo"))
                                            .build();
        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));

        assertEquals(R.drawable.ic_chrome, notification.icon);
        assertNotNull(((ImageView) compactView.findViewById(R.id.icon)).getDrawable());
        assertNotNull(((ImageView) bigView.findViewById(R.id.icon)).getDrawable());
        assertEquals("title", getIdenticalText(R.id.title, compactView, bigView));
        assertEquals("body", getIdenticalText(R.id.body, compactView, bigView));
        assertEquals("origin", getIdenticalText(R.id.origin, compactView, bigView));
        assertEquals("ticker", notification.tickerText.toString());
        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(1, notification.vibrate.length);
        assertEquals(100L, notification.vibrate[0]);
        assertSame(contentIntent, notification.contentIntent);
        assertSame(deleteIntent, notification.deleteIntent);

        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, "button", View.FIND_VIEWS_WITH_TEXT);
        assertEquals(2, buttons.size());
        assertEquals(View.VISIBLE, bigView.findViewById(R.id.button_divider).getVisibility());
        assertEquals(View.VISIBLE, bigView.findViewById(R.id.buttons).getVisibility());
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testZeroActionButtons() {
        Context context = getInstrumentation().getTargetContext();
        Notification notification = new CustomNotificationBuilder(context).build();
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, "button", View.FIND_VIEWS_WITH_TEXT);

        // When there are no buttons the container and divider must not be shown.
        assertTrue(buttons.isEmpty());
        assertEquals(View.GONE, bigView.findViewById(R.id.button_divider).getVisibility());
        assertEquals(View.GONE, bigView.findViewById(R.id.buttons).getVisibility());
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testMaxActionButtons() {
        Context context = getInstrumentation().getTargetContext();
        NotificationBuilder builder = new CustomNotificationBuilder(context)
                                              .addAction(0 /* iconId */, "button",
                                                      createIntent(context, "ActionButtonOne"))
                                              .addAction(0 /* iconId */, "button",
                                                      createIntent(context, "ActionButtonTwo"))
                                              .addAction(0 /* iconId */, "button",
                                                      createIntent(context, "ActionButtonThree"));
        try {
            builder.addAction(0 /* iconId */, "button", createIntent(context, "ActionButtonFour"));
            fail("This statement should not be reached as the previous statement should throw.");
        } catch (IllegalStateException e) {
            assertEquals("Cannot add more than 3 actions.", e.getMessage());
        }
        Notification notification = builder.build();
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, "button", View.FIND_VIEWS_WITH_TEXT);

        assertEquals("There is a maximum of 3 buttons", 3, buttons.size());
    }

    /**
     * Finds a TextView with the given id in each of the given views, and checks that they all
     * contain the same text.
     * @param id The id to find the TextView instances with.
     * @param views The views to find the TextView instances in.
     * @return The identical text.
     */
    private CharSequence getIdenticalText(int id, View... views) {
        CharSequence result = null;
        for (View view : views) {
            TextView textView = (TextView) view.findViewById(id);
            assertNotNull(textView);
            CharSequence text = textView.getText();
            if (result == null) {
                result = text;
            } else {
                assertEquals(result, text);
            }
        }
        return result;
    }

    private PendingIntent createIntent(Context context, String action) {
        Intent intent = new Intent("CustomNotificationBuilderTest." + action);
        return PendingIntent.getBroadcast(
                context, 0 /* requestCode */, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }
}
