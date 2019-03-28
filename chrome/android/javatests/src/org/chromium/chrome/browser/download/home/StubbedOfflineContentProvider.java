// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import static junit.framework.Assert.assertEquals;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.RenameResult;
import org.chromium.components.offline_items_collection.ShareCallback;
import org.chromium.components.offline_items_collection.VisualsCallback;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;

/** Stubs out the OfflineContentProvider. */
public class StubbedOfflineContentProvider implements OfflineContentProvider {
    private final CallbackHelper mAddObserverCallback;
    private final CallbackHelper mRemoveObserverCallback;
    private final CallbackHelper mDeleteItemCallback;
    private final ArrayList<OfflineItem> mItems;
    private OfflineContentProvider.Observer mObserver;

    public StubbedOfflineContentProvider() {
        mAddObserverCallback = new CallbackHelper();
        mRemoveObserverCallback = new CallbackHelper();
        mDeleteItemCallback = new CallbackHelper();
        mItems = new ArrayList<>();
    }

    public ArrayList<OfflineItem> getItems() {
        return mItems;
    }

    public void addItem(OfflineItem item) {
        mItems.add(item);
    }

    @Override
    public void addObserver(OfflineContentProvider.Observer addedObserver) {
        assertEquals(mObserver, null);
        mObserver = addedObserver;
        mAddObserverCallback.notifyCalled();
    }

    @Override
    public void removeObserver(OfflineContentProvider.Observer removedObserver) {
        assertEquals(mObserver, removedObserver);
        mObserver = null;
        mRemoveObserverCallback.notifyCalled();
    }

    @Override
    public void getItemById(ContentId id, Callback<OfflineItem> callback) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(null));
    }

    @Override
    public void getAllItems(Callback<ArrayList<OfflineItem>> callback) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(mItems));
    }

    @Override
    public void getVisualsForItem(ContentId id, VisualsCallback callback) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onVisualsAvailable(id, null));
    }

    @Override
    public void getShareInfoForItem(ContentId id, ShareCallback callback) {
        PostTask.postTask(
                UiThreadTaskTraits.DEFAULT, () -> callback.onShareInfoAvailable(id, null));
    }

    @Override
    public void removeItem(ContentId id) {
        for (OfflineItem item : mItems) {
            if (item.id.equals(id)) {
                mItems.remove(item);
                break;
            }
        }

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            mObserver.onItemRemoved(id);
            mDeleteItemCallback.notifyCalled();
        });
    }

    @Override
    public void openItem(@LaunchLocation int location, ContentId id) {}

    @Override
    public void pauseDownload(ContentId id) {}

    @Override
    public void resumeDownload(ContentId id, boolean hasUserGesture) {}

    @Override
    public void cancelDownload(ContentId id) {}

    @Override
    public void renameItem(ContentId id, String name, Callback<Integer /*RenameResult*/> callback) {
        PostTask.postTask(
                UiThreadTaskTraits.DEFAULT, () -> callback.onResult(RenameResult.SUCCESS));
    }
}
