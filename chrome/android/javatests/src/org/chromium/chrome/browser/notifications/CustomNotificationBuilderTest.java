// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.test.filters.SmallTest;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Instrumentation unit tests for CustomNotificationBuilder.
 */
public class CustomNotificationBuilderTest extends NativeLibraryTestBase {

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryNoBrowserProcess();
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testSetAll() {
        Context context = getInstrumentation().getTargetContext();

        PendingIntent contentIntent = createIntent(context, "Content");
        PendingIntent deleteIntent = createIntent(context, "Delete");

        Bitmap smallIcon =
                BitmapFactory.decodeResource(context.getResources(), R.drawable.ic_chrome);

        Bitmap largeIcon = Bitmap.createBitmap(
                new int[] {Color.RED}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        largeIcon = largeIcon.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);

        Bitmap actionIcon = Bitmap.createBitmap(
                new int[] {Color.WHITE}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        actionIcon = actionIcon.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);

        Notification notification = new CustomNotificationBuilder(context)
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
                                            .addButtonAction(actionIcon, "button",
                                                    createIntent(context, "ActionButtonOne"))
                                            .addButtonAction(actionIcon, "button",
                                                    createIntent(context, "ActionButtonTwo"))
                                            .addSettingsAction(0 /* iconId */, "settings",
                                                    createIntent(context, "SettingsButton"))
                                            .build();

        assertSmallNotificationIconAsExpected(context, notification, smallIcon);
        assertLargeNotificationIconAsExpected(context, notification, largeIcon);

        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));

        assertEquals("title", getIdenticalText(R.id.title, compactView, bigView));
        assertEquals("body", getIdenticalText(R.id.body, compactView, bigView));
        assertEquals("origin", getIdenticalText(R.id.origin, compactView, bigView));

        assertEquals("title", NotificationTestUtil.getExtraTitle(notification));
        assertEquals("body", NotificationTestUtil.getExtraText(notification));
        assertEquals("origin", NotificationTestUtil.getExtraSubText(notification));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            assertEquals(
                    NotificationConstants.GROUP_WEB_PREFIX + "origin", notification.getGroup());
        }

        assertEquals("ticker", notification.tickerText.toString());
        assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        assertEquals(1, notification.vibrate.length);
        assertEquals(100L, notification.vibrate[0]);
        assertSame(contentIntent, notification.contentIntent);
        assertSame(deleteIntent, notification.deleteIntent);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Notification.publicVersion was added in Android L.
            assertNotNull(notification.publicVersion);
            assertEquals("origin", Build.VERSION.SDK_INT <= Build.VERSION_CODES.M
                            ? NotificationTestUtil.getExtraTitle(notification.publicVersion)
                            : NotificationTestUtil.getExtraSubText(notification.publicVersion));
        }

        // The regular actions and the settings action are added together in the notification
        // actions array, so they can be exposed on e.g. Wear and custom lockscreens.
        assertEquals(3, NotificationTestUtil.getActions(notification).length);

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
                                                  .addButtonAction(null /* iconBitmap */, "button",
                                                          createIntent(context, "ActionButtonOne"))
                                                  .addButtonAction(null /* iconBitmap */, "button",
                                                          createIntent(context, "ActionButtonTwo"));
        try {
            builder.addButtonAction(
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
    public void testPaintIcons() {
        Context context = getInstrumentation().getTargetContext();

        Bitmap largeIcon = Bitmap.createBitmap(
                new int[] {Color.RED}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        largeIcon = largeIcon.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);

        Bitmap smallIcon = Bitmap.createBitmap(
                new int[] {Color.RED}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        smallIcon = smallIcon.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);

        Bitmap actionIcon = Bitmap.createBitmap(
                new int[] {Color.RED}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        actionIcon = actionIcon.copy(Bitmap.Config.ARGB_8888, true /* isMutable */);

        Notification notification = new CustomNotificationBuilder(context)
                                            .setLargeIcon(largeIcon)
                                            .setSmallIcon(smallIcon)
                                            .addButtonAction(actionIcon, "button",
                                                    createIntent(context, "ActionButton"))
                                            .build();

        // The large icon should be unchanged.
        assertLargeNotificationIconAsExpected(context, notification, largeIcon);

        // Small icons should be painted white.
        Bitmap whiteIcon = Bitmap.createBitmap(
                new int[] {Color.WHITE}, 1 /* width */, 1 /* height */, Bitmap.Config.ARGB_8888);
        assertSmallNotificationIconAsExpected(context, notification, whiteIcon);

        // Action icons should be painted white.
        assertEquals(1, NotificationTestUtil.getActions(notification).length);
        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        ImageView actionIconView = (ImageView) bigView.findViewById(R.id.button_icon);
        Bitmap actionIconBitmap = ((BitmapDrawable) actionIconView.getDrawable()).getBitmap();
        assertTrue(whiteIcon.sameAs(actionIconBitmap));
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
                        .addButtonAction(null /* iconBitmap */, createString('e', maxLength + 1),
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

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testGeneratesLargeIconFromOriginWhenNoLargeIconProvided() {
        Context context = getInstrumentation().getTargetContext();
        NotificationBuilderBase notificationBuilder =
                new CustomNotificationBuilder(context).setOrigin("https://www.google.com");

        Notification notification = notificationBuilder.build();

        Bitmap expectedIcon = NotificationBuilderBase.createIconGenerator(context.getResources())
                                      .generateIconForUrl("https://www.google.com");

        assertLargeNotificationIconAsExpected(context, notification, expectedIcon);
    }

    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testGeneratesLargeIconFromOriginWhenLargeIconProvidedIsNull() {
        Context context = getInstrumentation().getTargetContext();
        NotificationBuilderBase notificationBuilder = new CustomNotificationBuilder(context)
                                                              .setOrigin("https://www.chromium.org")
                                                              .setLargeIcon(null);

        Notification notification = notificationBuilder.build();

        Bitmap expectedIcon = NotificationBuilderBase.createIconGenerator(context.getResources())
                                      .generateIconForUrl("https://www.chromium.org");

        assertLargeNotificationIconAsExpected(context, notification, expectedIcon);
    }

    /**
     * Tests that adding a text action results in a notification action with a RemoteInput attached.
     *
     * Note that the action buttons in custom layouts will not trigger an inline reply, but we can
     * still check the notification's action properties since these are used on Android Wear.
     */
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT_WATCH)
    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH) // RemoteInputs were only added in KITKAT_WATCH.
    @SmallTest
    @Feature({"Browser", "Notifications"})
    public void testAddTextActionSetsRemoteInput() {
        Context context = getInstrumentation().getTargetContext();
        NotificationBuilderBase notificationBuilder = new CustomNotificationBuilder(
                context).addTextAction(null, "Action Title", null, "Placeholder");

        Notification notification = notificationBuilder.build();

        assertEquals(1, notification.actions.length);
        assertEquals("Action Title", notification.actions[0].title);
        assertNotNull(notification.actions[0].getRemoteInputs());
        assertEquals(1, notification.actions[0].getRemoteInputs().length);
        assertEquals("Placeholder", notification.actions[0].getRemoteInputs()[0].getLabel());
    }

    private static void assertLargeNotificationIconAsExpected(
            Context context, Notification notification, Bitmap expectedIcon) {
        // 1. Check large icon property on the notification.

        Bitmap icon = NotificationTestUtil.getLargeIconFromNotification(context, notification);
        assertNotNull(icon);
        assertTrue(expectedIcon.sameAs(icon));

        // 2. Check the large icon in the custom layouts.

        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        Drawable compactViewIcon = ((ImageView) compactView.findViewById(R.id.icon)).getDrawable();
        assertNotNull(compactViewIcon);
        assertTrue(expectedIcon.sameAs(((BitmapDrawable) compactViewIcon).getBitmap()));

        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        Drawable bigViewIcon = ((ImageView) bigView.findViewById(R.id.icon)).getDrawable();
        assertNotNull(bigViewIcon);
        assertTrue(expectedIcon.sameAs(((BitmapDrawable) bigViewIcon).getBitmap()));
    }

    private static void assertSmallNotificationIconAsExpected(
            Context context, Notification notification, Bitmap expectedIcon) {
        // 1. Check small icon property on the notification, for M+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Bitmap icon =
                    NotificationTestUtil.getBitmapFromIcon(context, notification.getSmallIcon());
            assertNotNull(icon);
            assertTrue(expectedIcon.sameAs(icon));
        }

        // 2. Check the small icon in the custom layouts.

        int smallIconId = CustomNotificationBuilder.useMaterial() ? R.id.small_icon_overlay
                                                                  : R.id.small_icon_footer;
        View compactView = notification.contentView.apply(context, new LinearLayout(context));
        Drawable compactViewIcon =
                ((ImageView) compactView.findViewById(smallIconId)).getDrawable();
        assertNotNull(compactViewIcon);
        assertTrue(expectedIcon.sameAs(((BitmapDrawable) compactViewIcon).getBitmap()));

        View bigView = notification.bigContentView.apply(context, new LinearLayout(context));
        Drawable bigViewIcon = ((ImageView) bigView.findViewById(smallIconId)).getDrawable();
        assertNotNull(bigViewIcon);
        assertTrue(expectedIcon.sameAs(((BitmapDrawable) bigViewIcon).getBitmap()));
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
