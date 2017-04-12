// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.is;

import android.app.NotificationManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;

/**
 * Tests that ChannelsUpdater correctly initializes channels on the notification manager.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ChannelsUpdaterTest {
    private MockNotificationManagerProxy mMockNotificationManager;
    private InMemorySharedPreferences mMockSharedPreferences;
    private ChannelsInitializer mChannelsInitializer;

    @Before
    public void setUp() throws Exception {
        mMockNotificationManager = new MockNotificationManagerProxy();
        mChannelsInitializer = new ChannelsInitializer(mMockNotificationManager);
        mMockSharedPreferences = new InMemorySharedPreferences();
    }

    @Test
    public void testShouldUpdateChannels_returnsFalsePreO() throws Exception {
        ChannelsUpdater updater = new ChannelsUpdater(
                false /* isAtLeastO */, mMockSharedPreferences, mChannelsInitializer, 0);
        assertThat(updater.shouldUpdateChannels(), is(false));
    }

    @Test
    public void testShouldUpdateChannels_returnsTrueIfOAndNoSavedVersionInPrefs() throws Exception {
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mMockSharedPreferences, mChannelsInitializer, 0);
        assertThat(updater.shouldUpdateChannels(), is(true));
    }

    @Test
    public void testShouldUpdateChannels_returnsTrueIfOAndDifferentVersionInPrefs()
            throws Exception {
        mMockSharedPreferences.edit().putInt(ChannelsUpdater.CHANNELS_VERSION_KEY, 4).apply();
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mMockSharedPreferences, mChannelsInitializer, 5);
        assertThat(updater.shouldUpdateChannels(), is(true));
    }

    @Test
    public void testShouldUpdateChannels_returnsFalseIfOAndSameVersionInPrefs() throws Exception {
        mMockSharedPreferences.edit().putInt(ChannelsUpdater.CHANNELS_VERSION_KEY, 3).apply();
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mMockSharedPreferences, mChannelsInitializer, 3);
        assertThat(updater.shouldUpdateChannels(), is(false));
    }

    @Test
    public void testUpdateChannels_noopPreO() throws Exception {
        ChannelsUpdater updater = new ChannelsUpdater(
                false /* isAtLeastO */, mMockSharedPreferences, mChannelsInitializer, 21);
        updater.updateChannels();

        assertThat(mMockNotificationManager.getChannels().size(), is(0));
        assertThat(mMockSharedPreferences.getInt(ChannelsUpdater.CHANNELS_VERSION_KEY, -1), is(-1));
    }

    @Test
    public void testUpdateChannels_createsExpectedChannelsAndUpdatesPref() throws Exception {
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mMockSharedPreferences, mChannelsInitializer, 21);
        updater.updateChannels();

        assertThat(mMockNotificationManager.getChannels().size(), is(greaterThan(0)));
        assertThat(mMockNotificationManager.getNotificationChannelIds(),
                containsInAnyOrder(ChannelsInitializer.CHANNEL_ID_BROWSER,
                        ChannelsInitializer.CHANNEL_ID_DOWNLOADS,
                        ChannelsInitializer.CHANNEL_ID_INCOGNITO,
                        ChannelsInitializer.CHANNEL_ID_SITES,
                        ChannelsInitializer.CHANNEL_ID_MEDIA));
        assertThat(mMockSharedPreferences.getInt(ChannelsUpdater.CHANNELS_VERSION_KEY, -1), is(21));
    }

    // Suppressed in order to construct the old channels with invalid parameters.
    @SuppressWarnings("WrongConstant")
    @Test
    public void testUpdateChannels_deletesOldChannelsAndCreatesExpectedOnes() throws Exception {
        mMockNotificationManager.createNotificationChannel(new ChannelsInitializer.Channel(
                "OldChannel", 8292304, NotificationManager.IMPORTANCE_HIGH,
                ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));
        mMockNotificationManager.createNotificationChannel(
                new ChannelsInitializer.Channel("AnotherOldChannel", 8292304,
                        NotificationManager.IMPORTANCE_LOW, "OldChannelGroup"));
        assertThat(mMockNotificationManager.getNotificationChannelIds(),
                containsInAnyOrder("OldChannel", "AnotherOldChannel"));

        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mMockSharedPreferences, mChannelsInitializer, 12);
        updater.updateChannels();

        assertThat(mMockNotificationManager.getNotificationChannelIds(),
                containsInAnyOrder(ChannelsInitializer.CHANNEL_ID_BROWSER,
                        ChannelsInitializer.CHANNEL_ID_DOWNLOADS,
                        ChannelsInitializer.CHANNEL_ID_INCOGNITO,
                        ChannelsInitializer.CHANNEL_ID_SITES,
                        ChannelsInitializer.CHANNEL_ID_MEDIA));
    }
}