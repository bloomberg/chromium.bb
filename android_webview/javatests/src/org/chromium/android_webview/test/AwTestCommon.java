// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.Instrumentation;
import android.content.Context;
import android.util.AndroidRuntimeException;

import org.junit.Assert;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwTestBase.PopupInfo;
import org.chromium.android_webview.test.AwTestBase.TestDependencyFactory;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.util.TestWebServer;

import java.lang.annotation.Annotation;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

// TODO(yolandyan): move this class to its test rule once JUnit4 migration is over
final class AwTestCommon {
    static final long WAIT_TIMEOUT_MS = scaleTimeout(15000);

    static final int CHECK_INTERVAL = 100;

    private static final String TAG = "AwTestCommon";

    private static final Pattern MAYBE_QUOTED_STRING = Pattern.compile("^(\"?)(.*)\\1$");

    // The browser context needs to be a process-wide singleton.
    private AwBrowserContext mBrowserContext;

    private final AwTestCommonCallback mCallback;

    AwTestCommon(AwTestCommonCallback callback) {
        mCallback = callback;
    }

    void setUp() throws Exception {
        if (mCallback.needsAwBrowserContextCreated()) {
            createAwBrowserContext();
        }
        if (mCallback.needsBrowserProcessStarted()) {
            startBrowserProcess();
        }
    }

    void createAwBrowserContext() {
        if (mBrowserContext != null) {
            throw new AndroidRuntimeException("There should only be one browser context.");
        }
        mCallback.launchActivity(); // The Activity must be launched in order to load native code
        final InMemorySharedPreferences prefs = new InMemorySharedPreferences();
        final Context appContext =
                mCallback.getInstrumentation().getTargetContext().getApplicationContext();
        mCallback.getInstrumentation().runOnMainSync(() -> mBrowserContext =
                mCallback.createAwBrowserContextOnUiThread(prefs, appContext));
    }

    void startBrowserProcess() throws Exception {
        // The Activity must be launched in order for proper webview statics to be setup.
        mCallback.launchActivity();
        mCallback.getInstrumentation().runOnMainSync(() -> AwBrowserProcess.start());
    }

    <R> R runTestOnUiThreadAndGetResult(Callable<R> callable) throws Exception {
        FutureTask<R> task = new FutureTask<R>(callable);
        mCallback.getInstrumentation().runOnMainSync(task);
        return task.get();
    }

    void enableJavaScriptOnUiThread(final AwContents awContents) {
        mCallback.getInstrumentation().runOnMainSync(
                () -> awContents.getSettings().setJavaScriptEnabled(true));
    }

    void setNetworkAvailableOnUiThread(final AwContents awContents, final boolean networkUp) {
        mCallback.getInstrumentation().runOnMainSync(
                () -> awContents.setNetworkAvailable(networkUp));
    }

    void loadUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url) throws Exception {
        loadUrlSync(awContents, onPageFinishedHelper, url, null);
    }

    void loadUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url, final Map<String, String> extraHeaders) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url, extraHeaders);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    void loadUrlSyncAndExpectError(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            CallbackHelper onReceivedErrorHelper, final String url) throws Exception {
        int onErrorCallCount = onReceivedErrorHelper.getCallCount();
        int onFinishedCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url);
        onReceivedErrorHelper.waitForCallback(
                onErrorCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        onPageFinishedHelper.waitForCallback(
                onFinishedCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    void loadUrlAsync(final AwContents awContents, final String url) throws Exception {
        loadUrlAsync(awContents, url, null);
    }

    void loadUrlAsync(
            final AwContents awContents, final String url, final Map<String, String> extraHeaders) {
        mCallback.getInstrumentation().runOnMainSync(() -> awContents.loadUrl(url, extraHeaders));
    }

    void postUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url, byte[] postData) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        postUrlAsync(awContents, url, postData);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    void postUrlAsync(final AwContents awContents, final String url, byte[] postData)
            throws Exception {
        class PostUrl implements Runnable {
            byte[] mPostData;
            public PostUrl(byte[] postData) {
                mPostData = postData;
            }
            @Override
            public void run() {
                awContents.postUrl(url, mPostData);
            }
        }
        mCallback.getInstrumentation().runOnMainSync(new PostUrl(postData));
    }

    void loadDataSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String data, final String mimeType, final boolean isBase64Encoded)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataAsync(awContents, data, mimeType, isBase64Encoded);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    void loadDataSyncWithCharset(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String data, final String mimeType, final boolean isBase64Encoded,
            final String charset) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        mCallback.getInstrumentation().runOnMainSync(
                () -> awContents.loadUrl(LoadUrlParams.createLoadDataParams(
                        data, mimeType, isBase64Encoded, charset)));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    void loadDataAsync(final AwContents awContents, final String data, final String mimeType,
            final boolean isBase64Encoded) throws Exception {
        mCallback.getInstrumentation().runOnMainSync(
                () -> awContents.loadData(data, mimeType, isBase64Encoded ? "base64" : null));
    }

    void loadDataWithBaseUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String data, final String mimeType, final boolean isBase64Encoded,
            final String baseUrl, final String historyUrl) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataWithBaseUrlAsync(awContents, data, mimeType, isBase64Encoded, baseUrl, historyUrl);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    void loadDataWithBaseUrlAsync(final AwContents awContents, final String data,
            final String mimeType, final boolean isBase64Encoded, final String baseUrl,
            final String historyUrl) throws Throwable {
        mCallback.runOnUiThread(() -> awContents.loadDataWithBaseURL(
                baseUrl, data, mimeType, isBase64Encoded ? "base64" : null, historyUrl));
    }

    void reloadSync(final AwContents awContents, CallbackHelper onPageFinishedHelper)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        mCallback.getInstrumentation().runOnMainSync(
                () -> awContents.getNavigationController().reload(true));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    void stopLoading(final AwContents awContents) {
        mCallback.getInstrumentation().runOnMainSync(() -> awContents.stopLoading());
    }

    void waitForVisualStateCallback(final AwContents awContents) throws Exception {
        final CallbackHelper ch = new CallbackHelper();
        final int chCount = ch.getCallCount();
        mCallback.getInstrumentation().runOnMainSync(() -> {
            final long requestId = 666;
            awContents.insertVisualStateCallback(
                    requestId, new AwContents.VisualStateCallback() {
                        @Override
                        public void onComplete(long id) {
                            Assert.assertEquals(requestId, id);
                            ch.notifyCalled();
                        }
                    });
        });
        ch.waitForCallback(chCount);
    }

    void insertVisualStateCallbackOnUIThread(final AwContents awContents, final long requestId,
            final AwContents.VisualStateCallback callback) {
        mCallback.getInstrumentation().runOnMainSync(
                () -> awContents.insertVisualStateCallback(requestId, callback));
    }

    void waitForPixelColorAtCenterOfView(final AwContents awContents,
            final AwTestContainerView testContainerView, final int expectedColor) throws Exception {
        pollUiThread(
                () -> GraphicsTestUtils.getPixelColorAtCenterOfView(awContents, testContainerView)
                        == expectedColor);
    }

    AwTestContainerView createAwTestContainerView(final AwContentsClient awContentsClient) {
        return createAwTestContainerView(awContentsClient, false, null);
    }

    AwTestContainerView createAwTestContainerView(final AwContentsClient awContentsClient,
            boolean supportsLegacyQuirks, final TestDependencyFactory testDependencyFactory) {
        AwTestContainerView testContainerView = createDetachedAwTestContainerView(
                awContentsClient, supportsLegacyQuirks, testDependencyFactory);
        mCallback.getActivity().addView(testContainerView);
        testContainerView.requestFocus();
        return testContainerView;
    }

    AwBrowserContext getAwBrowserContext() {
        return mBrowserContext;
    }

    AwTestContainerView createDetachedAwTestContainerView(final AwContentsClient awContentsClient) {
        return createDetachedAwTestContainerView(awContentsClient, false, null);
    }

    AwTestContainerView createDetachedAwTestContainerView(final AwContentsClient awContentsClient,
            boolean supportsLegacyQuirks, TestDependencyFactory testDependencyFactory) {
        if (testDependencyFactory == null) {
            testDependencyFactory = mCallback.createTestDependencyFactory();
        }
        boolean allowHardwareAcceleration = isHardwareAcceleratedTest();
        final AwTestContainerView testContainerView =
                testDependencyFactory.createAwTestContainerView(
                        mCallback.getActivity(), allowHardwareAcceleration);

        AwSettings awSettings = testDependencyFactory.createAwSettings(
                mCallback.getActivity(), supportsLegacyQuirks);
        AwContents awContents = testDependencyFactory.createAwContents(mBrowserContext,
                testContainerView, testContainerView.getContext(),
                testContainerView.getInternalAccessDelegate(),
                testContainerView.getNativeDrawGLFunctorFactory(), awContentsClient, awSettings,
                testDependencyFactory);
        testContainerView.initialize(awContents);
        return testContainerView;
    }

    boolean isHardwareAcceleratedTest() {
        return !mCallback.testMethodHasAnnotation(DisableHardwareAccelerationForTest.class);
    }

    AwTestContainerView createAwTestContainerViewOnMainSync(final AwContentsClient client)
            throws Exception {
        return createAwTestContainerViewOnMainSync(client, false, null);
    }

    AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client, final boolean supportsLegacyQuirks) {
        return createAwTestContainerViewOnMainSync(client, supportsLegacyQuirks, null);
    }

    AwTestContainerView createAwTestContainerViewOnMainSync(final AwContentsClient client,
            final boolean supportsLegacyQuirks, final TestDependencyFactory testDependencyFactory) {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> createAwTestContainerView(
                client, supportsLegacyQuirks, testDependencyFactory));
    }

    void destroyAwContentsOnMainSync(final AwContents awContents) {
        if (awContents == null) return;
        mCallback.getInstrumentation().runOnMainSync(() -> awContents.destroy());
    }

    String getTitleOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(() -> awContents.getTitle());
    }

    AwSettings getAwSettingsOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(() -> awContents.getSettings());
    }

    String maybeStripDoubleQuotes(String raw) {
        Assert.assertNotNull(raw);
        Matcher m = MAYBE_QUOTED_STRING.matcher(raw);
        Assert.assertTrue(m.matches());
        return m.group(2);
    }

    String executeJavaScriptAndWaitForResult(final AwContents awContents,
            TestAwContentsClient viewClient, final String code) throws Exception {
        return JSUtils.executeJavaScriptAndWaitForResult(mCallback.getInstrumentation(), awContents,
                viewClient.getOnEvaluateJavaScriptResultHelper(), code);
    }

    /**
     * Executes JavaScript code within the given ContentView to get the text content in
     * document body. Returns the result string without double quotes.
     */
    String getJavaScriptResultBodyTextContent(
            final AwContents awContents, final TestAwContentsClient viewClient) throws Exception {
        String raw = executeJavaScriptAndWaitForResult(
                awContents, viewClient, "document.body.textContent");
        return maybeStripDoubleQuotes(raw);
    }

    static void pollInstrumentationThread(final Callable<Boolean> callable) throws Exception {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return callable.call();
                } catch (Throwable e) {
                    Log.e(TAG, "Exception while polling.", e);
                    return false;
                }
            }
        }, WAIT_TIMEOUT_MS, CHECK_INTERVAL);
    }

    void pollUiThread(final Callable<Boolean> callable) throws Exception {
        pollInstrumentationThread(() -> runTestOnUiThreadAndGetResult(callable));
    }

    void clearCacheOnUiThread(final AwContents awContents, final boolean includeDiskFiles)
            throws Exception {
        mCallback.getInstrumentation().runOnMainSync(() -> awContents.clearCache(includeDiskFiles));
    }

    float getScaleOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(() -> awContents.getPageScaleFactor());
    }

    float getPixelScaleOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(() -> awContents.getScale());
    }

    boolean canZoomInOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(() -> awContents.canZoomIn());
    }

    boolean canZoomOutOnUiThread(final AwContents awContents) throws Exception {
        return runTestOnUiThreadAndGetResult(() -> awContents.canZoomOut());
    }

    void killRenderProcessOnUiThreadAsync(final AwContents awContents) throws Exception {
        mCallback.getInstrumentation().runOnMainSync(() -> awContents.killRenderProcess());
    }

    void triggerPopup(final AwContents parentAwContents,
            TestAwContentsClient parentAwContentsClient, TestWebServer testWebServer,
            String mainHtml, String popupHtml, String popupPath, String triggerScript)
            throws Exception {
        enableJavaScriptOnUiThread(parentAwContents);
        mCallback.getInstrumentation().runOnMainSync(() -> {
            parentAwContents.getSettings().setSupportMultipleWindows(true);
            parentAwContents.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
        });

        final String parentUrl = testWebServer.setResponse("/popupParent.html", mainHtml, null);
        if (popupHtml != null) {
            testWebServer.setResponse(popupPath, popupHtml, null);
        } else {
            testWebServer.setResponseWithNoContentStatus(popupPath);
        }

        parentAwContentsClient.getOnCreateWindowHelper().setReturnValue(true);
        loadUrlSync(parentAwContents, parentAwContentsClient.getOnPageFinishedHelper(), parentUrl);

        TestAwContentsClient.OnCreateWindowHelper onCreateWindowHelper =
                parentAwContentsClient.getOnCreateWindowHelper();
        int currentCallCount = onCreateWindowHelper.getCallCount();
        parentAwContents.evaluateJavaScriptForTests(triggerScript, null);
        onCreateWindowHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    PopupInfo connectPendingPopup(final AwContents parentAwContents) throws Exception {
        TestAwContentsClient popupContentsClient;
        AwTestContainerView popupContainerView;
        final AwContents popupContents;
        popupContentsClient = new TestAwContentsClient();
        popupContainerView = createAwTestContainerViewOnMainSync(popupContentsClient);
        popupContents = popupContainerView.getAwContents();
        enableJavaScriptOnUiThread(popupContents);

        mCallback.getInstrumentation().runOnMainSync(
                () -> parentAwContents.supplyContentsForPopup(popupContents));

        OnPageFinishedHelper onPageFinishedHelper = popupContentsClient.getOnPageFinishedHelper();
        int finishCallCount = onPageFinishedHelper.getCallCount();
        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                popupContentsClient.getOnReceivedTitleHelper();
        int titleCallCount = onReceivedTitleHelper.getCallCount();

        onPageFinishedHelper.waitForCallback(
                finishCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        onReceivedTitleHelper.waitForCallback(
                titleCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        return new PopupInfo(popupContentsClient, popupContainerView, popupContents);
    }

    interface AwTestCommonCallback {
        Instrumentation getInstrumentation();
        AwTestRunnerActivity getActivity();
        AwTestRunnerActivity launchActivity();
        void runOnUiThread(Runnable runnable) throws Throwable;
        boolean testMethodHasAnnotation(Class<? extends Annotation> clazz);
        AwBrowserContext createAwBrowserContextOnUiThread(
                InMemorySharedPreferences prefs, Context appContext);
        TestDependencyFactory createTestDependencyFactory();
        boolean needsAwBrowserContextCreated();
        boolean needsBrowserProcessStarted();
    }
}
