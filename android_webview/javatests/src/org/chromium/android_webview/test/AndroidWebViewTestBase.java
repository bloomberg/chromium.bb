// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Instrumentation;
import android.content.Context;
import android.test.ActivityInstrumentationTestCase2;

import junit.framework.Assert;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.ui.gfx.ActivityNativeWindow;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * A base class for android_webview tests.
 */
public class AndroidWebViewTestBase
        extends ActivityInstrumentationTestCase2<AndroidWebViewTestRunnerActivity> {
    protected static int WAIT_TIMEOUT_SECONDS = 15;
    private static final int CHECK_INTERVAL = 100;

    public AndroidWebViewTestBase() {
        super(AndroidWebViewTestRunnerActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        final Context context = getActivity();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                AwTestResourceProvider.registerResources(context);
                ContentViewCore.initChromiumBrowserProcess(
                        context, ContentView.MAX_RENDERERS_SINGLE_PROCESS);
            }
        });
    }

    /**
     * Runs a {@link Callable} on the main thread, blocking until it is
     * complete, and returns the result. Calls
     * {@link Instrumentation#waitForIdleSync()} first to help avoid certain
     * race conditions.
     *
     * @param <R> Type of result to return
     */
    public <R> R runTestOnUiThreadAndGetResult(Callable<R> callable)
            throws Throwable {
        FutureTask<R> task = new FutureTask<R>(callable);
        getInstrumentation().waitForIdleSync();
        getInstrumentation().runOnMainSync(task);
        try {
            return task.get();
        } catch (ExecutionException e) {
            // Unwrap the cause of the exception and re-throw it.
            throw e.getCause();
        }
    }

    protected void enableJavaScriptOnUiThread(final AwContents awContents) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.getContentViewCore().getContentSettings().setJavaScriptEnabled(true);
            }
        });
    }

    /**
     * Loads url on the UI thread and blocks until onPageFinished is called.
     */
    protected void loadUrlSync(final AwContents awContents,
                               CallbackHelper onPageFinishedHelper,
                               final String url) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    protected void loadUrlSyncAndExpectError(final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            CallbackHelper onReceivedErrorHelper,
            final String url) throws Throwable {
        int onErrorCallCount = onReceivedErrorHelper.getCallCount();
        int onFinishedCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url);
        onReceivedErrorHelper.waitForCallback(onErrorCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
        onPageFinishedHelper.waitForCallback(onFinishedCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    protected void loadUrlAsync(final AwContents awContents,
                                final String url) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                awContents.loadUrl(new LoadUrlParams(url));
            }
        });
    }

    /**
     * Posts url on the UI thread and blocks until onPageFinished is called.
     */
    protected void postUrlSync(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, final String url,
            byte[] postData) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        postUrlAsync(awContents, url, postData);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    protected void postUrlAsync(final AwContents awContents,
            final String url, byte[] postData) throws Throwable {
        class PostUrl implements Runnable {
            byte[] mPostData;
            public PostUrl(byte[] postData) {
                mPostData = postData;
            }
            @Override
            public void run() {
                awContents.loadUrl(LoadUrlParams.createLoadHttpPostParams(url,
                        mPostData));
            }
        }
        runTestOnUiThread(new PostUrl(postData));
    }

    /**
     * Loads data on the UI thread and blocks until onPageFinished is called.
     */
    protected void loadDataSync(final AwContents awContents,
                                CallbackHelper onPageFinishedHelper,
                                final String data, final String mimeType,
                                final boolean isBase64Encoded) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataAsync(awContents, data, mimeType, isBase64Encoded);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    protected void loadDataSyncWithCharset(final AwContents awContents,
                                           CallbackHelper onPageFinishedHelper,
                                           final String data, final String mimeType,
                                           final boolean isBase64Encoded, final String charset)
            throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                awContents.loadUrl(LoadUrlParams.createLoadDataParams(
                        data, mimeType, isBase64Encoded, charset));
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_SECONDS,
                TimeUnit.SECONDS);
    }

    /**
     * Loads data on the UI thread but does not block.
     */
    protected void loadDataAsync(final AwContents awContents, final String data,
                                 final String mimeType, final boolean isBase64Encoded)
            throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                awContents.loadUrl(LoadUrlParams.createLoadDataParams(
                        data, mimeType, isBase64Encoded));
            }
        });
    }

    protected AwTestContainerView createAwTestContainerView(
            final AwContentsClient awContentsClient) {
        return createAwTestContainerView(new AwTestContainerView(getActivity()),
                awContentsClient);
    }

    protected AwTestContainerView createAwTestContainerView(
            final AwTestContainerView testContainerView,
            final AwContentsClient awContentsClient) {
        testContainerView.initialize(new AwContents(testContainerView,
                testContainerView.getInternalAccessDelegate(),
                awContentsClient, new ActivityNativeWindow(getActivity()), false));
        getActivity().addView(testContainerView);
        testContainerView.requestFocus();
        return testContainerView;
    }

    protected AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client) throws Exception {
        final AtomicReference<AwTestContainerView> testContainerView =
                new AtomicReference<AwTestContainerView>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                testContainerView.set(createAwTestContainerView(client));
            }
        });
        return testContainerView.get();
    }

    protected void destroyAwContentsOnMainSync(final AwContents awContents) {
        if (awContents == null) return;
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.destroy();
            }
        });
    }

    protected String getTitleOnUiThread(final AwContents awContents) throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return awContents.getContentViewCore().getTitle();
            }
        });
    }

    protected ContentSettings getContentSettingsOnUiThread(
            final AwContents awContents) throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<ContentSettings>() {
            @Override
            public ContentSettings call() throws Exception {
                return awContents.getContentViewCore().getContentSettings();
            }
        });
    }

    protected AwSettings getAwSettingsOnUiThread(
            final AwContents awContents) throws Throwable {
        return runTestOnUiThreadAndGetResult(new Callable<AwSettings>() {
            @Override
            public AwSettings call() throws Exception {
                return awContents.getSettings();
            }
        });
    }

    /**
     * Executes the given snippet of JavaScript code within the given ContentView. Returns the
     * result of its execution in JSON format.
     */
    protected String executeJavaScriptAndWaitForResult(final AwContents awContents,
            TestAwContentsClient viewClient, final String code) throws Throwable {
        return JSUtils.executeJavaScriptAndWaitForResult(this, awContents,
                viewClient.getOnEvaluateJavaScriptResultHelper(),
                code);
    }

    /**
     * Similar to CriteriaHelper.pollForCriteria but runs the callable on the UI thread.
     * Note that exceptions are treated as failure.
     */
    protected boolean pollOnUiThread(final Callable<Boolean> callable) throws Throwable {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return runTestOnUiThreadAndGetResult(callable);
                } catch (Throwable e) {
                    return false;
                }
            }
        });
    }

    /**
     * Clears the resource cache. Note that the cache is per-application, so this will clear the
     * cache for all WebViews used.
     */
    protected void clearCacheOnUiThread(
            final AwContents awContents,
            final boolean includeDiskFiles) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
              awContents.clearCache(includeDiskFiles);
            }
        });
    }
}
