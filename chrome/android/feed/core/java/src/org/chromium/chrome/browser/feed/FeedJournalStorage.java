// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.storage.CommitResult;
import com.google.android.libraries.feed.host.storage.JournalMutation;
import com.google.android.libraries.feed.host.storage.JournalStorage;

import org.chromium.base.Callback;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of {@link JournalStorage} that persisits data on native side.
 */
public class FeedJournalStorage implements JournalStorage {
    private FeedJournalBridge mFeedJournalBridge;

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

    private static class ReadCallback implements Callback<List<String>> {
        private final Consumer < Result < List<byte[]>>> mConsumer;

        public ReadCallback(Consumer < Result < List<byte[]>>> consumer) {
            mConsumer = consumer;
        }

        @Override
        public void onResult(List<String> entries) {
            // TODO(gangwu): Need to handle failure case.
            List<byte[]> journal = new ArrayList<byte[]>();
            for (String entry : entries) {
                journal.add(entry.getBytes());
            }
            mConsumer.accept(Result.success(journal));
        }
    }

    private static class CommitCallback implements Callback<Boolean> {
        private final Consumer<CommitResult> mConsumer;

        CommitCallback(Consumer<CommitResult> consumer) {
            mConsumer = consumer;
        }

        @Override
        public void onResult(Boolean result) {
            if (result) {
                mConsumer.accept(CommitResult.SUCCESS);
            } else {
                mConsumer.accept(CommitResult.FAILURE);
            }
        }
    }

    /**
     * Creates a {@link FeedJournalStorage} for storing journals for the current user.
     *
     * @param bridge {@link FeedJournalBridge} implementation can handle journal storage request.
     */
    public FeedJournalStorage(FeedJournalBridge bridge) {
        mFeedJournalBridge = bridge;
    }

    /** Cleans up {@link FeedJournalStorage}. */
    public void destroy() {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.destroy();
        mFeedJournalBridge = null;
    }

    @Override
    public void read(String journalName, Consumer < Result < List<byte[]>>> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.loadJournal(journalName, new ReadCallback(consumer));
    }

    @Override
    public void commit(JournalMutation mutation, Consumer<CommitResult> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.commitJournalMutation(mutation, new CommitCallback(consumer));
    }

    @Override
    public void exists(String journalName, Consumer<Result<Boolean>> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.doesJournalExist(journalName, new StorageCallback<Boolean>(consumer));
    }

    @Override
    public void getAllJournals(Consumer < Result < List<String>>> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.loadAllJournalKeys(new StorageCallback<List<String>>(consumer));
    }

    @Override
    public void deleteAll(Consumer<CommitResult> consumer) {
        assert mFeedJournalBridge != null;
        mFeedJournalBridge.deleteAllJournals(new CommitCallback(consumer));
    }
}
