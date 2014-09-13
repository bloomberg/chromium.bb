// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Instrumentation;
import android.content.Context;
import android.test.ActivityInstrumentationTestCase2;
import android.util.Log;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.content.browser.ContentSettings;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A base class for android_webview tests.
 */
public class AwTestBase
        extends ActivityInstrumentationTestCase2<AwTestRunnerActivity> {
    public static final long WAIT_TIMEOUT_MS = scaleTimeout(15000);
    public static final int CHECK_INTERVAL = 100;
    private static final String TAG = "AwTestBase";

    public AwTestBase() {
        super(AwTestRunnerActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (needsBrowserProcessStarted()) {
            final Context context = getActivity();
            getInstrumentation().runOnMainSync(new Runnable() {
                @Override
                public void run() {
                    AwBrowserProcess.start(context);
                }
            });
        }
    }

    /* Override this to return false if the test doesn't want the browser startup sequence to
     * be run automatically.
     */
    protected boolean needsBrowserProcessStarted() {
        return true;
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
            throws Exception {
        FutureTask<R> task = new FutureTask<R>(callable);
        getInstrumentation().waitForIdleSync();
        getInstrumentation().runOnMainSync(task);
        return task.get();
    }

    public void enableJavaScriptOnUiThread(final AwContents awContents) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.getSettings().setJavaScriptEnabled(true);
            }
        });
    }

    public void setNetworkAvailableOnUiThread(final AwContents awContents,
            final boolean networkUp) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.setNetworkAvailable(networkUp);
            }
        });
    }

    /**
     * Loads url on the UI thread and blocks until onPageFinished is called.
     */
    public void loadUrlSync(final AwContents awContents,
                               CallbackHelper onPageFinishedHelper,
                               final String url) throws Exception {
        loadUrlSync(awContents, onPageFinishedHelper, url, null);
    }

    public void loadUrlSync(final AwContents awContents,
                               CallbackHelper onPageFinishedHelper,
                               final String url,
                               final Map<String, String> extraHeaders) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url, extraHeaders);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    public void loadUrlSyncAndExpectError(final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            CallbackHelper onReceivedErrorHelper,
            final String url) throws Exception {
        int onErrorCallCount = onReceivedErrorHelper.getCallCount();
        int onFinishedCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url);
        onReceivedErrorHelper.waitForCallback(onErrorCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
        onPageFinishedHelper.waitForCallback(onFinishedCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    public void loadUrlAsync(final AwContents awContents,
                                final String url) throws Exception {
        loadUrlAsync(awContents, url, null);
    }

    public void loadUrlAsync(final AwContents awContents,
                                final String url,
                                final Map<String, String> extraHeaders) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                LoadUrlParams params = new LoadUrlParams(url);
                params.setExtraHeaders(extraHeaders);
                awContents.loadUrl(params);
            }
        });
    }

    /**
     * Posts url on the UI thread and blocks until onPageFinished is called.
     */
    public void postUrlSync(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, final String url,
            byte[] postData) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        postUrlAsync(awContents, url, postData);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    public void postUrlAsync(final AwContents awContents,
            final String url, byte[] postData) throws Exception {
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
        getInstrumentation().runOnMainSync(new PostUrl(postData));
    }

    /**
     * Loads data on the UI thread and blocks until onPageFinished is called.
     */
    public void loadDataSync(final AwContents awContents,
                                CallbackHelper onPageFinishedHelper,
                                final String data, final String mimeType,
                                final boolean isBase64Encoded) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataAsync(awContents, data, mimeType, isBase64Encoded);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    public void loadDataSyncWithCharset(final AwContents awContents,
                                           CallbackHelper onPageFinishedHelper,
                                           final String data, final String mimeType,
                                           final boolean isBase64Encoded, final String charset)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.loadUrl(LoadUrlParams.createLoadDataParams(
                        data, mimeType, isBase64Encoded, charset));
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    /**
     * Loads data on the UI thread but does not block.
     */
    public void loadDataAsync(final AwContents awContents, final String data,
                                 final String mimeType, final boolean isBase64Encoded)
            throws Exception {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.loadUrl(LoadUrlParams.createLoadDataParams(
                        data, mimeType, isBase64Encoded));
            }
        });
    }

    public void loadDataWithBaseUrlSync(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, final String data, final String mimeType,
            final boolean isBase64Encoded, final String baseUrl,
            final String historyUrl) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataWithBaseUrlAsync(awContents, data, mimeType, isBase64Encoded, baseUrl, historyUrl);
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    public void loadDataWithBaseUrlAsync(final AwContents awContents,
            final String data, final String mimeType, final boolean isBase64Encoded,
            final String baseUrl, final String historyUrl) throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                awContents.loadUrl(LoadUrlParams.createLoadDataParamsWithBaseUrl(
                        data, mimeType, isBase64Encoded, baseUrl, historyUrl));
            }
        });
    }

    /**
     * Reloads the current page synchronously.
     */
    public void reloadSync(final AwContents awContents,
                              CallbackHelper onPageFinishedHelper) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.getNavigationController().reload(true);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS,
                TimeUnit.MILLISECONDS);
    }

    /**
     * Factory class used in creation of test AwContents instances.
     *
     * Test cases can provide subclass instances to the createAwTest* methods in order to create an
     * AwContents instance with injected test dependencies.
     */
    public static class TestDependencyFactory extends AwContents.DependencyFactory {
        public AwTestContainerView createAwTestContainerView(AwTestRunnerActivity activity) {
            return new AwTestContainerView(activity, false);
        }
        public AwSettings createAwSettings(Context context, boolean supportsLegacyQuirks) {
            return new AwSettings(context, false, supportsLegacyQuirks);
        }
    }

    protected TestDependencyFactory createTestDependencyFactory() {
        return new TestDependencyFactory();
    }

    public AwTestContainerView createAwTestContainerView(
            final AwContentsClient awContentsClient) {
        return createAwTestContainerView(awContentsClient, false);
    }

    public AwTestContainerView createAwTestContainerView(
            final AwContentsClient awContentsClient, boolean supportsLegacyQuirks) {
        AwTestContainerView testContainerView =
                createDetachedAwTestContainerView(awContentsClient, supportsLegacyQuirks);
        getActivity().addView(testContainerView);
        testContainerView.requestFocus();
        return testContainerView;
    }

    // The browser context needs to be a process-wide singleton.
    private AwBrowserContext mBrowserContext =
            new AwBrowserContext(new InMemorySharedPreferences());

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient) {
        return createDetachedAwTestContainerView(awContentsClient, false);
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient, boolean supportsLegacyQuirks) {
        final TestDependencyFactory testDependencyFactory = createTestDependencyFactory();
        final AwTestContainerView testContainerView =
            testDependencyFactory.createAwTestContainerView(getActivity());
        AwSettings awSettings = testDependencyFactory.createAwSettings(getActivity(),
                supportsLegacyQuirks);
        testContainerView.initialize(new AwContents(
                mBrowserContext, testContainerView, testContainerView.getContext(),
                testContainerView.getInternalAccessDelegate(),
                testContainerView.getNativeGLDelegate(), awContentsClient,
                awSettings, testDependencyFactory));
        return testContainerView;
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client) throws Exception {
        return createAwTestContainerViewOnMainSync(client, false);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client, final boolean supportsLegacyQuirks) throws Exception {
        final AtomicReference<AwTestContainerView> testContainerView =
                new AtomicReference<AwTestContainerView>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                testContainerView.set(createAwTestContainerView(client, supportsLegacyQuirks));
            }
        });
        return testContainerView.get();
    }

    public void destroyAwContentsOnMainSync(final AwContents awContents) {
        if (awContents == null) return;
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.destroy();
            }
        });
    }

    public String getTitleOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return awContents.getTitle();
            }
        });
    }

    public ContentSettings getContentSettingsOnUiThread(
            final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(new Callable<ContentSettings>() {
            @Override
            public ContentSettings call() throws Exception {
                return awContents.getContentViewCore().getContentSettings();
            }
        });
    }

    public AwSettings getAwSettingsOnUiThread(
            final AwContents awContents) throws Exception {
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
    public String executeJavaScriptAndWaitForResult(final AwContents awContents,
            TestAwContentsClient viewClient, final String code) throws Exception {
        return JSUtils.executeJavaScriptAndWaitForResult(this, awContents,
                viewClient.getOnEvaluateJavaScriptResultHelper(),
                code);
    }

    /**
     * Wrapper around CriteriaHelper.pollForCriteria. This uses AwTestBase-specifc timeouts and
     * treats timeouts and exceptions as test failures automatically.
     */
    public static void poll(final Callable<Boolean> callable) throws Exception {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return callable.call();
                } catch (Throwable e) {
                    Log.e(TAG, "Exception while polling.", e);
                    return false;
                }
            }
        }, WAIT_TIMEOUT_MS, CHECK_INTERVAL));
    }

    /**
     * Wrapper around {@link AwTestBase#poll()} but runs the callable on the UI thread.
     */
    public void pollOnUiThread(final Callable<Boolean> callable) throws Exception {
        poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return runTestOnUiThreadAndGetResult(callable);
            }
        });
    }

    /**
     * Clears the resource cache. Note that the cache is per-application, so this will clear the
     * cache for all WebViews used.
     */
    public void clearCacheOnUiThread(
            final AwContents awContents,
            final boolean includeDiskFiles) throws Exception {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.clearCache(includeDiskFiles);
            }
        });
    }

    /**
     * Returns pure page scale.
     */
    public float getScaleOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(new Callable<Float>() {
            @Override
            public Float call() throws Exception {
                return awContents.getPageScaleFactor();
            }
        });
    }

    /**
     * Returns page scale multiplied by the screen density.
     */
    public float getPixelScaleOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(new Callable<Float>() {
            @Override
            public Float call() throws Exception {
                return awContents.getScale();
            }
        });
    }

    /**
     * Returns whether a user can zoom the page in.
     */
    public boolean canZoomInOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return awContents.canZoomIn();
            }
        });
    }

    /**
     * Returns whether a user can zoom the page out.
     */
    public boolean canZoomOutOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return awContents.canZoomOut();
            }
        });
    }

}
