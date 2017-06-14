// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.notifications.channels;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;

import android.app.NotificationManager;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.notifications.NotificationChannelStatus;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationSettingsBridge;

import java.util.Arrays;

/**
 * Instrumentation unit tests for SiteChannelsManager.
 *
 * They run against the real NotificationManager so we can be sure Android does what we expect.
 *
 * Note that channels are persistent by Android so even if a channel is deleted, if it is recreated
 * with the same id then the previous properties will be restored, including whether the channel was
 * blocked. Thus some of these tests use different channel ids to avoid this problem.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SiteChannelsManagerTest {
    private NotificationManagerProxy mNotificationManagerProxy;
    private SiteChannelsManager mSiteChannelsManager;

    @Before
    public void setUp() throws Exception {
        Context mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mNotificationManagerProxy = new NotificationManagerProxyImpl(
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE));
        clearExistingSiteChannels(mNotificationManagerProxy);
        mSiteChannelsManager = new SiteChannelsManager(mNotificationManagerProxy);
    }

    private static void clearExistingSiteChannels(
            NotificationManagerProxy notificationManagerProxy) {
        for (Channel channel : notificationManagerProxy.getNotificationChannels()) {
            if (channel.getId().startsWith(ChannelDefinitions.CHANNEL_ID_PREFIX_SITES)
                    || (channel.getGroupId() != null
                               && channel.getGroupId().equals(
                                          ChannelDefinitions.CHANNEL_GROUP_ID_SITES))) {
                notificationManagerProxy.deleteNotificationChannel(channel.getId());
            }
        }
    }

    @Test
    @MinAndroidSdkLevel(26)
    @SmallTest
    public void testCreateSiteChannel_enabled() throws Exception {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", true);
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = mSiteChannelsManager.getSiteChannels()[0];
        assertThat(channel.getOrigin(), is("https://chromium.org"));
        assertThat(channel.getStatus(), is(NotificationChannelStatus.ENABLED));
    }

    @Test
    @MinAndroidSdkLevel(26)
    @SmallTest
    public void testCreateSiteChannel_disabled() throws Exception {
        mSiteChannelsManager.createSiteChannel("https://example.com", false);
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(1));
        NotificationSettingsBridge.SiteChannel channel = mSiteChannelsManager.getSiteChannels()[0];
        assertThat(channel.getOrigin(), is("https://example.com"));
        assertThat(channel.getStatus(), is(NotificationChannelStatus.BLOCKED));
    }

    @Test
    @MinAndroidSdkLevel(26)
    @SmallTest
    public void testDeleteSiteChannel_channelExists() throws Exception {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", true);
        mSiteChannelsManager.deleteSiteChannel("https://chromium.org");
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(0));
    }

    @Test
    @MinAndroidSdkLevel(26)
    @SmallTest
    public void testDeleteSiteChannel_channelDoesNotExist() throws Exception {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", true);
        mSiteChannelsManager.deleteSiteChannel("https://some-other-origin.org");
        assertThat(Arrays.asList(mSiteChannelsManager.getSiteChannels()), hasSize(1));
    }

    @Test
    @MinAndroidSdkLevel(26)
    @SmallTest
    public void testGetChannelStatus_channelCreatedAsEnabled() throws Exception {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", true);
        assertThat(mSiteChannelsManager.getChannelStatus("https://chromium.org"),
                matchesChannelStatus(NotificationChannelStatus.ENABLED));
    }

    @Test
    @MinAndroidSdkLevel(26)
    @SmallTest
    public void testGetChannelStatus_channelCreatedAsBlocked() throws Exception {
        assertThat(mSiteChannelsManager.getChannelStatus("https://foobar.com"),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
        mSiteChannelsManager.createSiteChannel("https://foobar.com", false);
        assertThat(mNotificationManagerProxy.getNotificationChannel("web:https://foobar.com")
                           .getImportance(),
                is(NotificationManager.IMPORTANCE_NONE));
        assertThat(mSiteChannelsManager.getChannelStatus("https://foobar.com"),
                matchesChannelStatus(NotificationChannelStatus.BLOCKED));
    }

    @Test
    @MinAndroidSdkLevel(26)
    @SmallTest
    public void testGetChannelStatus_channelNotCreated() throws Exception {
        assertThat(mSiteChannelsManager.getChannelStatus("https://chromium.org"),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
    }

    @Test
    @MinAndroidSdkLevel(26)
    @SmallTest
    public void testGetChannelStatus_channelCreatedThenDeleted() throws Exception {
        mSiteChannelsManager.createSiteChannel("https://chromium.org", true);
        mSiteChannelsManager.deleteSiteChannel("https://chromium.org");
        assertThat(mSiteChannelsManager.getChannelStatus("https://chromium.org"),
                matchesChannelStatus(NotificationChannelStatus.UNAVAILABLE));
    }

    private static Matcher<Integer> matchesChannelStatus(
            @NotificationChannelStatus final int status) {
        return new BaseMatcher<Integer>() {
            @Override
            public boolean matches(Object o) {
                return status == (int) o;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("NotificationChannelStatus." + statusToString(status));
            }

            @Override
            public void describeMismatch(final Object item, final Description description) {
                description.appendText(
                        "was NotificationChannelStatus." + statusToString((int) item));
            }
        };
    }

    private static String statusToString(@NotificationChannelStatus int status) {
        switch (status) {
            case NotificationChannelStatus.ENABLED:
                return "ENABLED";
            case NotificationChannelStatus.BLOCKED:
                return "BLOCKED";
            case NotificationChannelStatus.UNAVAILABLE:
                return "UNAVAILABLE";
        }
        return null;
    }
}