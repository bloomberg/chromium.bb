// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.storage.JournalMutation;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.List;

/**
 * Provides access to native implementations of journal storage.
 */
@JNINamespace("feed")
public class FeedJournalBridge {
    private long mNativeFeedJournalBridge;

    /**
     * Creates a {@link FeedJournalBridge} for accessing native journal storage
     * implementation for the current user, and initial native side bridge.
     */
    public FeedJournalBridge() {}

    /**
     * Initializes the native side of this bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public void init(Profile profile) {}

    /** Cleans up native half of this bridge. */
    public void destroy() {}

    /** Loads the journal and asynchronously returns the contents. */
    public void loadJournal(String journalName, Callback<List<String>> callback) {}

    /**
     * Commits the operations in {@link JournalMutation} in order and asynchronously reports the
     * {@link CommitResult}. If all the operations succeed, {@code callback} is called with a
     * success result. If any operation fails, {@code callback} is called with a failure result and
     * the remaining operations are not processed.
     */
    public void commitJournalMutation(JournalMutation mutation, Callback<Boolean> callback) {}

    /** Determines whether a journal exists and asynchronously responds. */
    public void doesJournalExist(String journalName, Callback<Boolean> callback) {}

    /** Asynchronously retrieve a list of all current journals' name. */
    public void loadAllJournalKeys(Callback<List<String>> callback) {}

    /** Delete all journals. Reports success or failure. */
    public void deleteAllJournals(Callback<Boolean> callback) {}
}
