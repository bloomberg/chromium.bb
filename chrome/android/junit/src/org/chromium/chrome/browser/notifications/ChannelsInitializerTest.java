// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;

import android.app.NotificationManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;

/**
 * Unit tests for ChannelsInitializer.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ChannelsInitializerTest {
    private ChannelsInitializer mChannelsInitializer;
    private MockNotificationManagerProxy mMockNotificationManager;

    @Before
    public void setUp() throws Exception {
        mMockNotificationManager = new MockNotificationManagerProxy();
        mChannelsInitializer = new ChannelsInitializer(mMockNotificationManager);
    }

    @Test
    public void testInitializeStartupChannels() throws Exception {
        mChannelsInitializer.initializeStartupChannels();
        assertThat(mMockNotificationManager.getNotificationChannelIds(),
                containsInAnyOrder(ChannelsInitializer.CHANNEL_ID_BROWSER,
                        ChannelsInitializer.CHANNEL_ID_DOWNLOADS,
                        ChannelsInitializer.CHANNEL_ID_INCOGNITO,
                        ChannelsInitializer.CHANNEL_ID_SITES,
                        ChannelsInitializer.CHANNEL_ID_MEDIA));
    }

    @Test
    public void testEnsureInitialized_browserChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelsInitializer.CHANNEL_ID_BROWSER);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));
        ChannelsInitializer.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelsInitializer.CHANNEL_ID_BROWSER));
        assertThat(
                channel.mNameResId, is(org.chromium.chrome.R.string.notification_category_browser));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.mGroupId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelsInitializer.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_downloadsChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelsInitializer.CHANNEL_ID_DOWNLOADS);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));
        ChannelsInitializer.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelsInitializer.CHANNEL_ID_DOWNLOADS));
        assertThat(channel.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_downloads));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.mGroupId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelsInitializer.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_incognitoChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelsInitializer.CHANNEL_ID_INCOGNITO);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));
        ChannelsInitializer.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelsInitializer.CHANNEL_ID_INCOGNITO));
        assertThat(channel.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_incognito));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.mGroupId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelsInitializer.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_mediaChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelsInitializer.CHANNEL_ID_MEDIA);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));
        ChannelsInitializer.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelsInitializer.CHANNEL_ID_MEDIA));
        assertThat(
                channel.mNameResId, is(org.chromium.chrome.R.string.notification_category_media));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.mGroupId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelsInitializer.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_sitesChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelsInitializer.CHANNEL_ID_SITES);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));

        ChannelsInitializer.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelsInitializer.CHANNEL_ID_SITES));
        assertThat(
                channel.mNameResId, is(org.chromium.chrome.R.string.notification_category_sites));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(channel.mGroupId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelsInitializer.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelsInitializer.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_multipleCalls() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelsInitializer.CHANNEL_ID_SITES);
        mChannelsInitializer.ensureInitialized(ChannelsInitializer.CHANNEL_ID_BROWSER);
        assertThat(mMockNotificationManager.getChannels().size(), is(2));
    }
}