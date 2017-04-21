// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
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
        mChannelsInitializer =
                new ChannelsInitializer(mMockNotificationManager, new ChannelDefinitions());
    }

    @Test
    public void testDeleteLegacyChannels_noopOnCurrentDefinitions() throws Exception {
        assertThat(mMockNotificationManager.getNotificationChannelIds(), is(empty()));

        mChannelsInitializer.deleteLegacyChannels();
        assertThat(mMockNotificationManager.getNotificationChannelIds(), is(empty()));

        mChannelsInitializer.initializeStartupChannels();
        assertThat(mMockNotificationManager.getNotificationChannelIds(), is(not(empty())));

        int nChannels = mMockNotificationManager.getNotificationChannelIds().size();
        mChannelsInitializer.deleteLegacyChannels();
        assertThat(mMockNotificationManager.getNotificationChannelIds(), hasSize(nChannels));
    }

    @Test
    public void testInitializeStartupChannels() throws Exception {
        mChannelsInitializer.initializeStartupChannels();
        assertThat(mMockNotificationManager.getNotificationChannelIds(),
                containsInAnyOrder(ChannelDefinitions.CHANNEL_ID_BROWSER,
                        ChannelDefinitions.CHANNEL_ID_DOWNLOADS,
                        ChannelDefinitions.CHANNEL_ID_INCOGNITO,
                        ChannelDefinitions.CHANNEL_ID_SITES, ChannelDefinitions.CHANNEL_ID_MEDIA));
    }

    @Test
    public void testEnsureInitialized_browserChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_BROWSER);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));
        ChannelDefinitions.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelDefinitions.CHANNEL_ID_BROWSER));
        assertThat(
                channel.mNameResId, is(org.chromium.chrome.R.string.notification_category_browser));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.mGroupId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelDefinitions.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_downloadsChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_DOWNLOADS);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));
        ChannelDefinitions.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelDefinitions.CHANNEL_ID_DOWNLOADS));
        assertThat(channel.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_downloads));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.mGroupId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelDefinitions.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_incognitoChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_INCOGNITO);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));
        ChannelDefinitions.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelDefinitions.CHANNEL_ID_INCOGNITO));
        assertThat(channel.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_incognito));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.mGroupId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelDefinitions.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_mediaChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_MEDIA);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));
        ChannelDefinitions.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelDefinitions.CHANNEL_ID_MEDIA));
        assertThat(
                channel.mNameResId, is(org.chromium.chrome.R.string.notification_category_media));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.mGroupId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelDefinitions.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_sitesChannel() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_SITES);

        assertThat(mMockNotificationManager.getChannels().size(), is(1));

        ChannelDefinitions.Channel channel = mMockNotificationManager.getChannels().get(0);
        assertThat(channel.mId, is(ChannelDefinitions.CHANNEL_ID_SITES));
        assertThat(
                channel.mNameResId, is(org.chromium.chrome.R.string.notification_category_sites));
        assertThat(channel.mImportance, is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(channel.mGroupId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));

        assertThat(mMockNotificationManager.getNotificationChannelGroups().size(), is(1));
        ChannelDefinitions.ChannelGroup group =
                mMockNotificationManager.getNotificationChannelGroups().get(0);
        assertThat(group.mId, is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
        assertThat(group.mNameResId,
                is(org.chromium.chrome.R.string.notification_category_group_general));
    }

    @Test
    public void testEnsureInitialized_multipleCalls() throws Exception {
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_SITES);
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_BROWSER);
        assertThat(mMockNotificationManager.getChannels().size(), is(2));
    }
}