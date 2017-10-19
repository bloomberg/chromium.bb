// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Utility functions for dealing with bookmarks in tests.
 */
public class BookmarkTestUtil {

    /**
     * Waits until the bookmark model is loaded, i.e. until
     * {@link BookmarkModel#isBookmarkModelLoaded()} is true.
     */
    public static void waitForBookmarkModelLoaded() throws InterruptedException {
        final BookmarkModel bookmarkModel = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<BookmarkModel>() {
                    @Override
                    public BookmarkModel call() throws Exception {
                        return new BookmarkModel();
                    }
                });

        final CallbackHelper loadedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                bookmarkModel.runAfterBookmarkModelLoaded(new Runnable() {
                    @Override
                    public void run() {
                        loadedCallback.notifyCalled();
                    }
                });
            }
        });

        try {
            loadedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Bookmark model did not load: Timeout.");
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                bookmarkModel.destroy();
            }
        });
    }
}
