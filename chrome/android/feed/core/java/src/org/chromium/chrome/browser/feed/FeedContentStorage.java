// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.storage.CommitResult;
import com.google.android.libraries.feed.host.storage.ContentMutation;
import com.google.android.libraries.feed.host.storage.ContentOperation;
import com.google.android.libraries.feed.host.storage.ContentOperation.Delete;
import com.google.android.libraries.feed.host.storage.ContentOperation.DeleteByPrefix;
import com.google.android.libraries.feed.host.storage.ContentOperation.Type;
import com.google.android.libraries.feed.host.storage.ContentOperation.Upsert;
import com.google.android.libraries.feed.host.storage.ContentStorage;

import org.chromium.base.Callback;

import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Implementation of {@link ContentStorage} that persisits data on native side.
 */
public class FeedContentStorage implements ContentStorage {
    private FeedStorageBridge mFeedStorageBridge;

    private static class StorageCallback<T> implements Callback<T> {
        private final Consumer<Result<T>> mConsumer;

        public StorageCallback(Consumer<Result<T>> consumer) {
            mConsumer = consumer;
        }

        @Override
        public void onResult(T data) {
            // TODO(gangwu): Need to handle failure case.
            mConsumer.accept(Result.success(data));
        }
    }

    private static class CommitCallback implements Callback<Boolean> {
        private final Consumer<CommitResult> mConsumer;
        private int mOperationsLeft;
        private boolean mSuccess;

        CommitCallback(Consumer<CommitResult> consumer, int operationsCount) {
            mConsumer = consumer;
            mOperationsLeft = operationsCount;
            mSuccess = true;
        }

        @Override
        public void onResult(Boolean result) {
            --mOperationsLeft;
            assert mOperationsLeft >= 0;

            mSuccess &= result.booleanValue();
            // TODO(gangwu): if |result| is failure, all other operation should halt immediately.
            if (mOperationsLeft == 0) {
                if (mSuccess) {
                    mConsumer.accept(CommitResult.SUCCESS);
                } else {
                    mConsumer.accept(CommitResult.FAILURE);
                }
            }
        }
    }

    /**
     * Creates a {@link FeedContentStorage} for storing content for the current user.
     *
     * @param bridge {@link FeedStorageBridge} implementation can handle content storage request.
     */
    public FeedContentStorage(FeedStorageBridge bridge) {
        mFeedStorageBridge = bridge;
    }

    /** Cleans up {@link FeedContentStorage}. */
    public void destroy() {
        assert mFeedStorageBridge != null;
        mFeedStorageBridge.destroy();
        mFeedStorageBridge = null;
    }

    @Override
    public void get(List<String> keys, Consumer < Result < Map<String, byte[]>>> consumer) {
        assert mFeedStorageBridge != null;
        mFeedStorageBridge.loadContent(keys, new StorageCallback<Map<String, byte[]>>(consumer));
    }

    @Override
    public void getAll(String prefix, Consumer < Result < Map<String, byte[]>>> consumer) {
        assert mFeedStorageBridge != null;
        mFeedStorageBridge.loadContentByPrefix(
                prefix, new StorageCallback<Map<String, byte[]>>(consumer));
    }

    @Override
    public void commit(ContentMutation mutation, Consumer<CommitResult> consumer) {
        assert mFeedStorageBridge != null;

        CommitCallback callback = new CommitCallback(consumer, mutation.getOperations().size());
        for (ContentOperation operation : mutation.getOperations()) {
            switch (operation.getType()) {
                case Type.UPSERT:
                    // TODO(gangwu): If upserts are continuous, we should conbine them into one
                    // array, and then send to native side.
                    Upsert upsert = (Upsert) operation;
                    String[] upsertKeys = {upsert.getKey()};
                    byte[][] upsertData = {upsert.getValue()};
                    mFeedStorageBridge.saveContent(upsertKeys, upsertData, callback);
                    break;
                case Type.DELETE:
                    // TODO(gangwu): If deletes are continuous, we should conbine them into one
                    // array, and then send to native side.
                    Delete delete = (Delete) operation;
                    List<String> deleteKeys = Collections.singletonList(delete.getKey());
                    mFeedStorageBridge.deleteContent(deleteKeys, callback);
                    break;
                case Type.DELETE_BY_PREFIX:
                    DeleteByPrefix deleteByPrefix = (DeleteByPrefix) operation;
                    String prefix = deleteByPrefix.getPrefix();
                    mFeedStorageBridge.deleteContentByPrefix(prefix, callback);
                    break;
                case Type.DELETE_ALL:
                    mFeedStorageBridge.deleteAllContent(callback);
                    break;
                default:
                    // Unsupport type of operations, so cannot performance the operation.
                    callback.onResult(false);
                    break;
            }
        }
    }

    @Override
    public void getAllKeys(Consumer < Result < List<String>>> consumer) {
        assert mFeedStorageBridge != null;
        mFeedStorageBridge.loadAllContentKeys(new StorageCallback<List<String>>(consumer));
    }
}
