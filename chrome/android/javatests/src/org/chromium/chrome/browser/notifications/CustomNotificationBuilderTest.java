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
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Instrumentation unit tests for CustomNotificationBuilder.
 */
@SuppressLint("NewApi") // For the |extras| property of Notification.
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

        Bitmap actionIcon = Bitmap.createBitmap(
                new int[] {Color.WHITE}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);

        Notification notification =
                new CustomNotificationBuilder(context)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setLargeIcon(largeIcon)
                        .setTitle("title")
                        .setBody("body")
                        .setOrigin("origin")
                        .setTicker("ticker")
                        .setDefaults(Notification.DEFAULT_ALL)
                        .setVibrate(new long[] {100L})
                        .setContentIntent(contentIntent)
                        .setDeleteIntent(deleteIntent)
                        .addAction(actionIcon, "button", createIntent(context, "ActionButtonOne"))
                        .addAction(actionIcon, "button", createIntent(context, "ActionButtonTwo"))
                        .addSettingsAction(
                                0 /* iconId */, "settings", createIntent(context, "SettingsButton"))
                        .build();
        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));

        assertEquals(R.drawable.ic_chrome, notification.icon);
        assertNotNull(((ImageView) compactView.findViewById(R.id.icon)).getDrawable());
        assertNotNull(((ImageView) bigView.findViewById(R.id.icon)).getDrawable());
        assertNotNull(notification.largeIcon);
        assertEquals("title", getIdenticalText(R.id.title, compactView, bigView));
        assertEquals("title", notification.extras.getString(Notification.EXTRA_TITLE));
        assertEquals("body", getIdenticalText(R.id.body, compactView, bigView));
        assertEquals("body", notification.extras.getString(Notification.EXTRA_TEXT));
        assertEquals("origin", getIdenticalText(R.id.origin, compactView, bigView));
        assertEquals("origin", notification.extras.getString(Notification.EXTRA_SUB_TEXT));
        assertEquals("ticker", notification.tickerText.toString());
        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(1, notification.vibrate.length);
        assertEquals(100L, notification.vibrate[0]);
        assertSame(contentIntent, notification.contentIntent);
        assertSame(deleteIntent, notification.deleteIntent);

        // The regular actions and the settings action are added together in the notification
        // actions array, so they can be exposed on e.g. Wear and custom lockscreens.
        assertEquals(3, notification.actions.length);
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
        NotificationBuilderBase builder = new CustomNotificationBuilder(context)
                                                  .addAction(null /* iconBitmap */, "button",
                                                          createIntent(context, "ActionButtonOne"))
                                                  .addAction(null /* iconBitmap */, "button",
                                                          createIntent(context, "ActionButtonTwo"));
        try {
            builder.addAction(
                    null /* iconBitmap */, "button", createIntent(context, "ActionButtonThree"));
            fail("This statement should not be reached as the previous statement should throw.");
        } catch (IllegalStateException e) {
            assertEquals("Cannot add more than 2 actions.", e.getMessage());
        }
        Notification notification = builder.build();
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, "button", View.FIND_VIEWS_WITH_TEXT);

        assertEquals("There is a maximum of 2 buttons", 2, buttons.size());
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testCharSequenceLimits() {
        Context context = getInstrumentation().getTargetContext();
        int maxLength = CustomNotificationBuilder.MAX_CHARSEQUENCE_LENGTH;
        Notification notification =
                new CustomNotificationBuilder(context)
                        .setTitle(createString('a', maxLength + 1))
                        .setBody(createString('b', maxLength + 1))
                        .setOrigin(createString('c', maxLength + 1))
                        .setTicker(createString('d', maxLength + 1))
                        .addAction(null /* iconBitmap */, createString('e', maxLength + 1),
                                createIntent(context, "ActionButtonOne"))
                        .build();
        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));

        assertEquals(maxLength, getIdenticalText(R.id.title, compactView, bigView).length());
        assertEquals(maxLength, getIdenticalText(R.id.body, compactView, bigView).length());
        assertEquals(maxLength, getIdenticalText(R.id.origin, compactView, bigView).length());
        assertEquals(maxLength, notification.tickerText.length());

        ArrayList<View> buttons = new ArrayList<>();
        bigView.findViewsWithText(buttons, createString('e', maxLength), View.FIND_VIEWS_WITH_TEXT);
        assertEquals(1, buttons.size());
        assertEquals(maxLength, ((Button) buttons.get(0)).getText().length());
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testCalculateMaxBodyLines() {
        assertEquals(7, CustomNotificationBuilder.calculateMaxBodyLines(-1000.0f));
        assertEquals(7, CustomNotificationBuilder.calculateMaxBodyLines(0.5f));
        assertEquals(7, CustomNotificationBuilder.calculateMaxBodyLines(1.0f));
        assertEquals(4, CustomNotificationBuilder.calculateMaxBodyLines(2.0f));
        assertEquals(1, CustomNotificationBuilder.calculateMaxBodyLines(1000.0f));
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testCalculateScaledPadding() {
        DisplayMetrics metrics = new DisplayMetrics();
        metrics.density = 10.0f;
        assertEquals(30, CustomNotificationBuilder.calculateScaledPadding(-1000.0f, metrics));
        assertEquals(30, CustomNotificationBuilder.calculateScaledPadding(0.5f, metrics));
        assertEquals(30, CustomNotificationBuilder.calculateScaledPadding(1.0f, metrics));
        assertEquals(20, CustomNotificationBuilder.calculateScaledPadding(1.1f, metrics));
        assertEquals(10, CustomNotificationBuilder.calculateScaledPadding(1.2f, metrics));
        assertEquals(0, CustomNotificationBuilder.calculateScaledPadding(1.3f, metrics));
        assertEquals(0, CustomNotificationBuilder.calculateScaledPadding(1000.0f, metrics));
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

    private static PendingIntent createIntent(Context context, String action) {
        Intent intent = new Intent("CustomNotificationBuilderTest." + action);
        return PendingIntent.getBroadcast(
                context, 0 /* requestCode */, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private static String createString(char character, int length) {
        char[] chars = new char[length];
        Arrays.fill(chars, character);
        return new String(chars);
    }
}
