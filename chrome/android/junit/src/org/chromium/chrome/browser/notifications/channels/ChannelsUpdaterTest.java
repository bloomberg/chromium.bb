// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.NotificationManager;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests that ChannelsUpdater correctly initializes channels on the notification manager.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ChannelsUpdaterTest {
    private NotificationManagerProxy mMockNotificationManager;
    private InMemorySharedPreferences mMockSharedPreferences;
    private ChannelsInitializer mChannelsInitializer;
    private Resources mMockResources;

    @Before
    public void setUp() throws Exception {
        mMockNotificationManager = new MockNotificationManagerProxy();

        // Mock the resources to prevent nullpointers on string resource lookups (none of these
        // tests need the real strings, if they did this would need to be an instrumentation test).
        mMockResources = mock(Resources.class);
        when(mMockResources.getString(any(Integer.class))).thenReturn("");

        mChannelsInitializer = new ChannelsInitializer(mMockNotificationManager, mMockResources);
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

        assertThat(mMockNotificationManager.getNotificationChannels(), hasSize(0));
        assertThat(mMockSharedPreferences.getInt(ChannelsUpdater.CHANNELS_VERSION_KEY, -1), is(-1));
    }

    @Test
    public void testUpdateChannels_createsExpectedChannelsAndUpdatesPref() throws Exception {
        ChannelsUpdater updater = new ChannelsUpdater(
                true /* isAtLeastO */, mMockSharedPreferences, mChannelsInitializer, 21);
        updater.updateChannels();

        assertThat(mMockNotificationManager.getNotificationChannels(), hasSize((greaterThan(0))));
        assertThat(getChannelIds(mMockNotificationManager.getNotificationChannels()),
                containsInAnyOrder(ChannelDefinitions.CHANNEL_ID_BROWSER,
                        ChannelDefinitions.CHANNEL_ID_DOWNLOADS,
                        ChannelDefinitions.CHANNEL_ID_INCOGNITO,
                        ChannelDefinitions.CHANNEL_ID_SITES, ChannelDefinitions.CHANNEL_ID_MEDIA));
        assertThat(mMockSharedPreferences.getInt(ChannelsUpdater.CHANNELS_VERSION_KEY, -1), is(21));
    }

    @Test
    public void testUpdateChannels_deletesLegacyChannelsAndCreatesExpectedOnes() throws Exception {
        // Set up any legacy channels.
        for (String id : ChannelDefinitions.getLegacyChannelIds()) {
            mMockNotificationManager.createNotificationChannel(
                    new Channel(id, id, NotificationManager.IMPORTANCE_LOW,
                            ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
        }

        ChannelsUpdater updater = new ChannelsUpdater(true /* isAtLeastO */, mMockSharedPreferences,
                new ChannelsInitializer(mMockNotificationManager, mMockResources), 12);
        updater.updateChannels();

        assertThat(getChannelIds(mMockNotificationManager.getNotificationChannels()),
                containsInAnyOrder(ChannelDefinitions.CHANNEL_ID_BROWSER,
                        ChannelDefinitions.CHANNEL_ID_DOWNLOADS,
                        ChannelDefinitions.CHANNEL_ID_INCOGNITO,
                        ChannelDefinitions.CHANNEL_ID_SITES, ChannelDefinitions.CHANNEL_ID_MEDIA));
    }

    private static List<String> getChannelIds(List<Channel> channels) {
        List<String> ids = new ArrayList<>();
        for (Channel ch : channels) {
            ids.add(ch.getId());
        }
        return ids;
    }
}