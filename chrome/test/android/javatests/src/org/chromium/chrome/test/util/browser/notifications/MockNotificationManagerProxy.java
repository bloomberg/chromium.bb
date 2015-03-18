// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.notifications;

import android.app.Notification;
import android.util.SparseArray;

import org.chromium.chrome.browser.notifications.NotificationManagerProxy;

import java.util.HashMap;
import java.util.Map;

/**
 * Mocked implementation of the NotificationManagerProxy. Imitates behavior of the Android
 * notification manager, and provides access to the displayed notifications for tests.
 */
public class MockNotificationManagerProxy implements NotificationManagerProxy {
    private final SparseArray<Notification> mNotifications;

    // Mapping between a textual tag and the index of the associated notification in
    // the |mNotifications| sparse array.
    private final Map<String, Integer> mTags;

    private int mMutationCount;

    public MockNotificationManagerProxy() {
        mNotifications = new SparseArray<>();
        mTags = new HashMap<>();
        mMutationCount = 0;
    }

    /**
     * Returns the notifications currently managed by the mocked notification manager.
     *
     * @return Sparse array of the managed notifications.
     */
    public SparseArray<Notification> getNotifications() {
        return mNotifications;
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
        if (mNotifications.indexOfKey(id) < 0) {
            throw new RuntimeException("Invalid notification id supplied.");
        }

        mNotifications.delete(id);
        mMutationCount++;
    }

    @Override
    public void cancel(String tag, int id) {
        if (!mTags.containsKey(tag)) {
            throw new RuntimeException("Invalid notification tag supplied.");
        }

        int notificationId = mTags.get(tag);
        if (notificationId != id) {
            throw new RuntimeException("Mismatch in notification ids for the given tag.");
        }

        mTags.remove(tag);

        assert mNotifications.indexOfKey(id) >= 0;
        mNotifications.delete(id);

        mMutationCount++;
    }

    @Override
    public void cancelAll() {
        mNotifications.clear();
        mTags.clear();

        mMutationCount++;
    }

    @Override
    public void notify(int id, Notification notification) {
        mNotifications.put(id, notification);
        mMutationCount++;
    }

    @Override
    public void notify(String tag, int id, Notification notification) {
        if (mTags.containsKey(tag)) {
            int notificationId = mTags.get(tag);

            assert mNotifications.indexOfKey(notificationId) >= 0;
            mNotifications.delete(notificationId);
        }

        mNotifications.put(id, notification);
        mTags.put(tag, id);

        mMutationCount++;
    }
}
