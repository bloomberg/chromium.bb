// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;

/** The JNI bridge for Android to receive notifications about history deletions. */
// TODO(crbug.com/964072): Write unit tests for this class.
public class HistoryDeletionBridge {
    /**
     * Allows derived class to listen to history deletions that pass through this bridge. The
     * HistoryDeletionInfo passed as a parameter is only valid for the duration of the method.
     */
    public interface Observer { void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo); }

    private static HistoryDeletionBridge sInstance;

    /**
     * @return Singleton instance for this class.
     */
    public static HistoryDeletionBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new HistoryDeletionBridge();
        }

        return sInstance;
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    HistoryDeletionBridge() {
        // This object is a singleton and therefore will be implicitly destroyed.
        nativeInit();
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo) {
        for (Observer observer : mObservers) observer.onURLsDeleted(historyDeletionInfo);
    }

    private native long nativeInit();
}
