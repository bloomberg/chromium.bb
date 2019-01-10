// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.support.annotation.Nullable;

import org.junit.Assert;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Test utility functions for OfflinePages. */
@JNINamespace("offline_pages")
public class OfflineTestUtil {
    // Gets all the URLs in the request queue.
    public static SavePageRequest[] getRequestsInQueue()
            throws TimeoutException, InterruptedException {
        final AtomicReference<SavePageRequest[]> result = new AtomicReference<>();
        final CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            nativeGetRequestsInQueue((SavePageRequest[] requests) -> {
                result.set(requests);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
        return result.get();
    }

    // Gets all available offline pages.
    public static List<OfflinePageItem> getAllPages()
            throws TimeoutException, InterruptedException {
        final AtomicReference<List<OfflinePageItem>> result =
                new AtomicReference<List<OfflinePageItem>>();
        final CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            nativeGetAllPages(new ArrayList<OfflinePageItem>(), (List<OfflinePageItem> items) -> {
                result.set(items);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
        return result.get();
    }

    // Returns the OfflinePageItem with the given clientId, or null if one doesn't exist.
    public static @Nullable OfflinePageItem getPageByClientId(ClientId clientId)
            throws TimeoutException, InterruptedException {
        ArrayList<OfflinePageItem> result = new ArrayList<OfflinePageItem>();
        for (OfflinePageItem item : getAllPages()) {
            if (item.getClientId().equals(clientId)) {
                return item;
            }
        }
        return null;
    }

    // Returns all OfflineItems provided by the OfflineContentProvider.
    public static List<OfflineItem> getOfflineItems()
            throws TimeoutException, InterruptedException {
        CallbackHelper finished = new CallbackHelper();
        final AtomicReference<ArrayList<OfflineItem>> result =
                new AtomicReference<ArrayList<OfflineItem>>();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineContentAggregatorFactory
                    .forProfile(Profile.getLastUsedProfile().getOriginalProfile())
                    .getAllItems(items -> {
                        result.set(items);
                        finished.notifyCalled();
                    });
        });
        finished.waitForCallback(0);
        return result.get();
    }

    // Waits for the offline model to initialize and returns an OfflinePageBridge.
    public static OfflinePageBridge getOfflinePageBridge()
            throws TimeoutException, InterruptedException {
        final CallbackHelper ready = new CallbackHelper();
        final AtomicReference<OfflinePageBridge> result = new AtomicReference<OfflinePageBridge>();
        ThreadUtils.runOnUiThread(() -> {
            OfflinePageBridge bridge =
                    OfflinePageBridge.getForProfile(Profile.getLastUsedProfile());
            if (bridge == null || bridge.isOfflinePageModelLoaded()) {
                result.set(bridge);
                ready.notifyCalled();
                return;
            }
            bridge.addObserver(new OfflinePageModelObserver() {
                @Override
                public void offlinePageModelLoaded() {
                    result.set(bridge);
                    ready.notifyCalled();
                    bridge.removeObserver(this);
                }
            });
        });
        ready.waitForCallback(0);
        Assert.assertTrue(result.get() != null);
        return result.get();
    }

    private static native void nativeGetRequestsInQueue(Callback<SavePageRequest[]> callback);
    private static native void nativeGetAllPages(
            List<OfflinePageItem> offlinePages, final Callback<List<OfflinePageItem>> callback);
}
