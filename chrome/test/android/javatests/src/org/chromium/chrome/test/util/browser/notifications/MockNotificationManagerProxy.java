// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.notifications;

import android.app.Notification;

import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.channels.Channel;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * Mocked implementation of the NotificationManagerProxy. Imitates behavior of the Android
 * notification manager, and provides access to the displayed notifications for tests.
 */
public class MockNotificationManagerProxy implements NotificationManagerProxy {
    private static final String KEY_SEPARATOR = ":";
    private List<Channel> mChannels;
    private Set<ChannelDefinitions.ChannelGroup> mNotificationChannelGroups;
    /**
     * Holds a notification and the arguments passed to #notify and #cancel.
     */
    public static class NotificationEntry {
        public final Notification notification;
        public final String tag;
        public final int id;

        NotificationEntry(Notification notification, String tag, int id) {
            this.notification = notification;
            this.tag = tag;
            this.id = id;
        }
    }

    // Maps (id:tag) to a NotificationEntry.
    private final Map<String, NotificationEntry> mNotifications;

    private int mMutationCount;

    public MockNotificationManagerProxy() {
        mNotifications = new LinkedHashMap<>();
        mMutationCount = 0;
        mChannels = new ArrayList<>();
        mNotificationChannelGroups = new HashSet<>();
    }

    /**
     * Returns the notifications currently managed by the mocked notification manager, in the order
     * in which they were shown, together with their associated tag and id. The list is not updated
     * when the internal data structure changes.
     *
     * @return List of the managed notifications.
     */
    public List<NotificationEntry> getNotifications() {
        return new ArrayList<>(mNotifications.values());
    }

    /**
     * May be used by tests to determine if a call has recently successfully been handled by the
     * notification manager. If the mutation count is higher than zero, it will be decremented by
     * one automatically.
     *
     * @return The number of recent mutations.
     */
    public int getMutationCountAndDecrement() {
        int mutationCount = mMutationCount;
        if (mutationCount > 0) mMutationCount--;

        return mutationCount;
    }

    @Override
    public void cancel(int id) {
        cancel(null /* tag */, id);
    }

    @Override
    public void cancel(@Nullable String tag, int id) {
        String key = makeKey(id, tag);
        mNotifications.remove(key);
        mMutationCount++;
    }

    @Override
    public void cancelAll() {
        mNotifications.clear();
        mMutationCount++;
    }

    @Override
    public void createNotificationChannel(Channel channel) {
        mChannels.add(channel);
    }

    @Override
    public void createNotificationChannelGroup(ChannelDefinitions.ChannelGroup channelGroup) {
        mNotificationChannelGroups.add(channelGroup);
    }

    @Override
    public List<Channel> getNotificationChannels() {
        return mChannels;
    }

    public List<ChannelDefinitions.ChannelGroup> getNotificationChannelGroups() {
        return new ArrayList<>(mNotificationChannelGroups);
    }

    @Override
    public void deleteNotificationChannel(String id) {
        for (Iterator<Channel> it = mChannels.iterator(); it.hasNext();) {
            Channel channel = it.next();
            if (id.equals(channel.getId())) it.remove();
        }
    }

    @Override
    public void notify(int id, Notification notification) {
        notify(null /* tag */, id, notification);
    }

    @Override
    public void notify(@Nullable String tag, int id, Notification notification) {
        mNotifications.put(makeKey(id, tag), new NotificationEntry(notification, tag, id));
        mMutationCount++;
    }

    private static String makeKey(int id, @Nullable String tag) {
        String key = Integer.toString(id);
        if (tag != null) key += KEY_SEPARATOR + tag;
        return key;
    }
}
