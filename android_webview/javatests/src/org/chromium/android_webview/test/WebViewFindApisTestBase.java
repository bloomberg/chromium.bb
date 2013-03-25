// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import org.chromium.android_webview.AwContents;

/**
 * Base class for WebView find-in-page API tests.
 */
public class WebViewFindApisTestBase extends AwTestBase {

    private static final String WOODCHUCK =
            "How much WOOD would a woodchuck chuck if a woodchuck could chuck wOoD?";

    private FindResultListener mFindResultListener;
    private AwContents mContents;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        try {
            mContents = loadContentsFromStringSync(WOODCHUCK);
        } catch (Throwable t) {
            throw new Exception(t);
        }
    }

    protected AwContents contents() {
        return mContents;
    }

    // Internal interface to intercept find results from AwContentsClient.
    private interface FindResultListener {
        public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
                boolean isDoneCounting);
    };

    private AwContents loadContentsFromStringSync(final String html) throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient() {
            @Override
            public void onFindResultReceived(int activeMatchOrdinal,
                    int numberOfMatches, boolean isDoneCounting) {
                if (mFindResultListener == null) return;
                mFindResultListener.onFindResultReceived(activeMatchOrdinal, numberOfMatches,
                        isDoneCounting);
            }
        };

        final AwContents contents =
                createAwTestContainerViewOnMainSync(contentsClient).getAwContents();
        final String data = "<html><head></head><body>" + html + "</body></html>";
        loadDataSync(contents, contentsClient.getOnPageFinishedHelper(),
                data, "text/html", false);
        return contents;
    }

    /**
     * Invokes findAllSync on the UI thread and returns the number of matches.
     *
     * @param searchString A string to search for.
     * @return The number of instances of the string that were found.
     * @throws Throwable
     */
    protected int findAllSyncOnUiThread(final String searchString)
            throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mContents.findAllSync(searchString);
            }
        });
    }

    /**
     * Invokes findAllAsync on the UI thread, blocks until find results are
     * received, and returns the number of matches.
     *
     * @param searchString A string to search for.
     * @return The number of instances of the string that were found.
     * @throws Throwable
     */
    protected int findAllAsyncOnUiThread(final String searchString)
            throws Throwable {
        final IntegerFuture future = new IntegerFuture() {
            @Override
            public void run() {
                mFindResultListener = new FindResultListener() {
                    @Override
                    public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
                            boolean isDoneCounting) {
                        if (isDoneCounting) set(numberOfMatches);
                    }
                };
                mContents.findAllAsync(searchString);
            }
        };
        runTestOnUiThread(future);
        return future.get(10, TimeUnit.SECONDS);
    }

    /**
     * Invokes findNext on the UI thread, blocks until find results are
     * received, and returns the ordinal of the highlighted match.
     *
     * @param forwards The direction to search as a boolean, with forwards
     *                 represented as true and backwards as false.
     * @return The ordinal of the highlighted match.
     * @throws Throwable
     */
    protected int findNextOnUiThread(final boolean forwards)
            throws Throwable {
        final IntegerFuture future = new IntegerFuture() {
            @Override
            public void run() {
                mFindResultListener = new FindResultListener() {
                    @Override
                    public void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
                            boolean isDoneCounting) {
                        if (isDoneCounting) set(activeMatchOrdinal);
                    }
                };
                mContents.findNext(forwards);
            }
        };
        runTestOnUiThread(future);
        return future.get(10, TimeUnit.SECONDS);
    }

    /**
     * Invokes clearMatches on the UI thread.
     *
     * @throws Throwable
     */
    protected void clearMatchesOnUiThread() throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mContents.clearMatches();
            }
        });
    }

    // Similar to java.util.concurrent.Future, but without the ability to cancel.
    private static abstract class IntegerFuture implements Runnable {
        private CountDownLatch mLatch = new CountDownLatch(1);
        private int mValue;

        @Override
        public abstract void run();

        /**
         * Gets the value of this Future, blocking for up to the specified
         * timeout for it become available. Throws a TimeoutException if the
         * timeout expires.
         */
        public int get(long timeout, TimeUnit unit) throws Throwable {
            if (!mLatch.await(timeout, unit)) {
                throw new TimeoutException();
            }
            return mValue;
        }

        /**
         * Sets the value of this Future.
         */
        protected void set(int value) {
            mValue = value;
            mLatch.countDown();
        }
    }
}
