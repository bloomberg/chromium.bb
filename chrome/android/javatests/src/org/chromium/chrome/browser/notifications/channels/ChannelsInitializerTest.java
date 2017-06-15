// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertThat;

import android.annotation.TargetApi;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BuildInfo;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Instrumentation tests for ChannelsInitializer.
 *
 * These are Android instrumentation tests so that resource strings can be accessed, and so that
 * we can test against the real NotificationManager implementation.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ChannelsInitializerTest {
    static final String MISCELLANEOUS_CHANNEL_ID = "miscellaneous";
    private ChannelsInitializer mChannelsInitializer;
    private NotificationManagerProxy mNotificationManagerProxy;
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mNotificationManagerProxy = new NotificationManagerProxyImpl(
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE));
        mChannelsInitializer =
                new ChannelsInitializer(mNotificationManagerProxy, mContext.getResources());
        // Delete any channels that may already have been initialized. Cleaning up here rather than
        // in tearDown in case tests running before these ones caused channels to be created.
        for (Channel channel : mNotificationManagerProxy.getNotificationChannels()) {
            if (!channel.getId().equals(MISCELLANEOUS_CHANNEL_ID)) {
                mNotificationManagerProxy.deleteNotificationChannel(channel.getId());
            }
        }
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testDeleteLegacyChannels_noopOnCurrentDefinitions() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        assertThat(getChannelsIgnoringMiscellaneous(), is(empty()));

        mChannelsInitializer.deleteLegacyChannels();
        assertThat(getChannelsIgnoringMiscellaneous(), is(empty()));

        mChannelsInitializer.initializeStartupChannels();
        assertThat(getChannelsIgnoringMiscellaneous(), is(not(empty())));

        int nChannels = getChannelsIgnoringMiscellaneous().size();
        mChannelsInitializer.deleteLegacyChannels();
        assertThat(getChannelsIgnoringMiscellaneous(), hasSize(nChannels));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testInitializeStartupChannels() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        mChannelsInitializer.initializeStartupChannels();
        List<String> notificationChannelIds = new ArrayList<>();
        for (Channel channel : getChannelsIgnoringMiscellaneous()) {
            notificationChannelIds.add(channel.getId());
        }
        assertThat(notificationChannelIds,
                containsInAnyOrder(ChannelDefinitions.CHANNEL_ID_BROWSER,
                        ChannelDefinitions.CHANNEL_ID_DOWNLOADS,
                        ChannelDefinitions.CHANNEL_ID_INCOGNITO,
                        ChannelDefinitions.CHANNEL_ID_SITES, ChannelDefinitions.CHANNEL_ID_MEDIA));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testInitializeStartupChannels_groupCreated() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        // Use a mock notification manager since the real one does not allow us to query which
        // groups have been created.
        MockNotificationManagerProxy mockNotificationManager = new MockNotificationManagerProxy();
        ChannelsInitializer channelsInitializer =
                new ChannelsInitializer(mockNotificationManager, mContext.getResources());
        channelsInitializer.initializeStartupChannels();
        assertThat(mockNotificationManager.getNotificationChannelGroups(), hasSize(1));
        assertThat(mockNotificationManager.getNotificationChannelGroups().get(0).mId,
                is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_browserChannel() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_BROWSER);

        assertThat(getChannelsIgnoringMiscellaneous(), hasSize(1));
        Channel channel = getChannelsIgnoringMiscellaneous().get(0);
        assertThat(channel.getId(), is(ChannelDefinitions.CHANNEL_ID_BROWSER));
        assertThat(channel.getName().toString(),
                is(mContext.getString(org.chromium.chrome.R.string.notification_category_browser)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroupId(), is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_downloadsChannel() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_DOWNLOADS);

        assertThat(getChannelsIgnoringMiscellaneous(), hasSize(1));
        Channel channel = getChannelsIgnoringMiscellaneous().get(0);
        assertThat(channel.getId(), is(ChannelDefinitions.CHANNEL_ID_DOWNLOADS));
        assertThat(channel.getName().toString(),
                is(mContext.getString(
                        org.chromium.chrome.R.string.notification_category_downloads)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroupId(), is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_incognitoChannel() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_INCOGNITO);

        assertThat(getChannelsIgnoringMiscellaneous(), hasSize(1));
        Channel channel = getChannelsIgnoringMiscellaneous().get(0);
        assertThat(channel.getId(), is(ChannelDefinitions.CHANNEL_ID_INCOGNITO));
        assertThat(channel.getName().toString(),
                is(mContext.getString(
                        org.chromium.chrome.R.string.notification_category_incognito)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroupId(), is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_mediaChannel() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_MEDIA);

        assertThat(getChannelsIgnoringMiscellaneous(), hasSize(1));
        Channel channel = getChannelsIgnoringMiscellaneous().get(0);
        assertThat(channel.getId(), is(ChannelDefinitions.CHANNEL_ID_MEDIA));
        assertThat(channel.getName().toString(),
                is(mContext.getString(org.chromium.chrome.R.string.notification_category_media)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_LOW));
        assertThat(channel.getGroupId(), is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_sitesChannel() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_SITES);

        assertThat(getChannelsIgnoringMiscellaneous(), hasSize(1));

        Channel channel = getChannelsIgnoringMiscellaneous().get(0);
        assertThat(channel.getId(), is(ChannelDefinitions.CHANNEL_ID_SITES));
        assertThat(channel.getName().toString(),
                is(mContext.getString(org.chromium.chrome.R.string.notification_category_sites)));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(channel.getGroupId(), is(ChannelDefinitions.CHANNEL_GROUP_ID_GENERAL));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_singleOriginSiteChannel() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        String origin = "https://example.com";
        mChannelsInitializer.ensureInitialized(SiteChannelsManager.toChannelId(origin));

        assertThat(getChannelsIgnoringMiscellaneous(), hasSize(1));

        Channel channel = getChannelsIgnoringMiscellaneous().get(0);
        assertThat(channel.getId(), is(SiteChannelsManager.toChannelId(origin)));
        assertThat(channel.getName().toString(), is("https://example.com"));
        assertThat(channel.getImportance(), is(NotificationManager.IMPORTANCE_DEFAULT));
        assertThat(channel.getGroupId(), is(ChannelDefinitions.CHANNEL_GROUP_ID_SITES));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/685808) Replace this with VERSION_CODES.O & remove isAtLeastO check below.
    @MinAndroidSdkLevel(Build.VERSION_CODES.N_MR1)
    @TargetApi(Build.VERSION_CODES.N_MR1)
    @Feature({"Browser", "Notifications"})
    public void testEnsureInitialized_multipleCalls() throws Exception {
        if (!BuildInfo.isAtLeastO()) return;
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_SITES);
        mChannelsInitializer.ensureInitialized(ChannelDefinitions.CHANNEL_ID_BROWSER);
        assertThat(getChannelsIgnoringMiscellaneous(), hasSize(2));
    }

    /**
     * Gets the current notification channels from the notification manager, except for any with
     * the id 'miscellaneous', which will be removed from the list before returning.
     *
     * (Android *might* add a Miscellaneous channel on our behalf, but we don't want to tie our
     * tests to its presence, as this could change).
     */
    private List<Channel> getChannelsIgnoringMiscellaneous() {
        List<Channel> channels = mNotificationManagerProxy.getNotificationChannels();
        for (Iterator<Channel> it = channels.iterator(); it.hasNext();) {
            Channel channel = it.next();
            if (channel.getId().equals(MISCELLANEOUS_CHANNEL_ID)) it.remove();
        }
        return channels;
    }
}