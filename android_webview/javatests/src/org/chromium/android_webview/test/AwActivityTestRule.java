// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.Instrumentation;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwTestBase.PopupInfo;
import org.chromium.android_webview.test.AwTestBase.TestDependencyFactory;
import org.chromium.android_webview.test.AwTestCommon.AwTestCommonCallback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.net.test.util.TestWebServer;

import java.lang.annotation.Annotation;
import java.util.Map;
import java.util.concurrent.Callable;

/** Custom ActivityTestRunner for WebView instrumentation tests */
public class AwActivityTestRule
        extends ActivityTestRule<AwTestRunnerActivity> implements AwTestCommonCallback {
    public static final long WAIT_TIMEOUT_MS = scaleTimeout(15000);

    public static final int CHECK_INTERVAL = 100;

    private Description mCurrentTestDescription;

    private final AwTestCommon mTestCommon;

    public AwActivityTestRule() {
        super(AwTestRunnerActivity.class, /* initialTouchMode */ false, /* launchActivity */ false);
        mTestCommon = new AwTestCommon(this);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        mCurrentTestDescription = description;
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                base.evaluate();
            }
        }, description);
    }

    public void setUp() throws Exception {
        mTestCommon.setUp();
    }

    @Override
    public AwTestRunnerActivity launchActivity() {
        if (getActivity() == null) {
            return launchActivity(null);
        }
        return getActivity();
    }

    @Override
    public AwBrowserContext createAwBrowserContextOnUiThread(
            InMemorySharedPreferences prefs, Context appContext) {
        return new AwBrowserContext(prefs, appContext);
    }

    @Override
    public TestDependencyFactory createTestDependencyFactory() {
        return new TestDependencyFactory();
    }

    /**
     * Override this to return false if the test doesn't want to create an
     * AwBrowserContext automatically.
     */
    @Override
    public boolean needsAwBrowserContextCreated() {
        return true;
    }

    /**
     * Override this to return false if the test doesn't want the browser
     * startup sequence to be run automatically.
     *
     * @return Whether the instrumentation test requires the browser process to
     *         already be started.
     */
    @Override
    public boolean needsBrowserProcessStarted() {
        return true;
    }

    public void createAwBrowserContext() {
        mTestCommon.createAwBrowserContext();
    }

    public void startBrowserProcess() throws Exception {
        mTestCommon.startBrowserProcess();
    }

    /**
     * Runs a {@link Callable} on the main thread, blocking until it is
     * complete, and returns the result. Calls
     * {@link Instrumentation#waitForIdleSync()} first to help avoid certain
     * race conditions.
     *
     * @param <R> Type of result to return
     */
    public <R> R runTestOnUiThreadAndGetResult(Callable<R> callable) throws Exception {
        return mTestCommon.runTestOnUiThreadAndGetResult(callable);
    }

    public void enableJavaScriptOnUiThread(final AwContents awContents) {
        mTestCommon.enableJavaScriptOnUiThread(awContents);
    }

    public void setNetworkAvailableOnUiThread(
            final AwContents awContents, final boolean networkUp) {
        mTestCommon.setNetworkAvailableOnUiThread(awContents, networkUp);
    }

    /**
     * Loads url on the UI thread and blocks until onPageFinished is called.
     */
    public void loadUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url) throws Exception {
        mTestCommon.loadUrlSync(awContents, onPageFinishedHelper, url);
    }

    public void loadUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url, final Map<String, String> extraHeaders) throws Exception {
        mTestCommon.loadUrlSync(awContents, onPageFinishedHelper, url, extraHeaders);
    }

    public void loadUrlSyncAndExpectError(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, CallbackHelper onReceivedErrorHelper,
            final String url) throws Exception {
        mTestCommon.loadUrlSyncAndExpectError(
                awContents, onPageFinishedHelper, onReceivedErrorHelper, url);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    public void loadUrlAsync(final AwContents awContents, final String url) throws Exception {
        mTestCommon.loadUrlAsync(awContents, url);
    }

    public void loadUrlAsync(
            final AwContents awContents, final String url, final Map<String, String> extraHeaders) {
        mTestCommon.loadUrlAsync(awContents, url, extraHeaders);
    }

    /**
     * Posts url on the UI thread and blocks until onPageFinished is called.
     */
    public void postUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url, byte[] postData) throws Exception {
        mTestCommon.postUrlSync(awContents, onPageFinishedHelper, url, postData);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    public void postUrlAsync(final AwContents awContents, final String url, byte[] postData)
            throws Exception {
        mTestCommon.postUrlAsync(awContents, url, postData);
    }

    /**
     * Loads data on the UI thread and blocks until onPageFinished is called.
     */
    public void loadDataSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String data, final String mimeType, final boolean isBase64Encoded)
            throws Exception {
        mTestCommon.loadDataSync(awContents, onPageFinishedHelper, data, mimeType, isBase64Encoded);
    }

    public void loadDataSyncWithCharset(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, final String data, final String mimeType,
            final boolean isBase64Encoded, final String charset) throws Exception {
        mTestCommon.loadDataSyncWithCharset(
                awContents, onPageFinishedHelper, data, mimeType, isBase64Encoded, charset);
    }

    /**
     * Loads data on the UI thread but does not block.
     */
    public void loadDataAsync(final AwContents awContents, final String data, final String mimeType,
            final boolean isBase64Encoded) throws Exception {
        mTestCommon.loadDataAsync(awContents, data, mimeType, isBase64Encoded);
    }

    public void loadDataWithBaseUrlSync(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, final String data, final String mimeType,
            final boolean isBase64Encoded, final String baseUrl, final String historyUrl)
            throws Throwable {
        mTestCommon.loadDataWithBaseUrlSync(awContents, onPageFinishedHelper, data, mimeType,
                isBase64Encoded, baseUrl, historyUrl);
    }

    public void loadDataWithBaseUrlAsync(final AwContents awContents, final String data,
            final String mimeType, final boolean isBase64Encoded, final String baseUrl,
            final String historyUrl) throws Throwable {
        mTestCommon.loadDataWithBaseUrlAsync(
                awContents, data, mimeType, isBase64Encoded, baseUrl, historyUrl);
    }

    /**
     * Reloads the current page synchronously.
     */
    public void reloadSync(final AwContents awContents, CallbackHelper onPageFinishedHelper)
            throws Exception {
        mTestCommon.reloadSync(awContents, onPageFinishedHelper);
    }

    /**
     * Stops loading on the UI thread.
     */
    public void stopLoading(final AwContents awContents) {
        mTestCommon.stopLoading(awContents);
    }

    public void waitForVisualStateCallback(final AwContents awContents) throws Exception {
        mTestCommon.waitForVisualStateCallback(awContents);
    }

    public void insertVisualStateCallbackOnUIThread(final AwContents awContents,
            final long requestId, final AwContents.VisualStateCallback callback) {
        mTestCommon.insertVisualStateCallbackOnUIThread(awContents, requestId, callback);
    }

    // Waits for the pixel at the center of AwContents to color up into expectedColor.
    // Note that this is a stricter condition that waiting for a visual state callback,
    // as visual state callback only indicates that *something* has appeared in WebView.
    public void waitForPixelColorAtCenterOfView(final AwContents awContents,
            final AwTestContainerView testContainerView, final int expectedColor) throws Exception {
        mTestCommon.waitForPixelColorAtCenterOfView(awContents, testContainerView, expectedColor);
    }

    public AwTestContainerView createAwTestContainerView(final AwContentsClient awContentsClient) {
        return mTestCommon.createAwTestContainerView(awContentsClient);
    }

    public AwTestContainerView createAwTestContainerView(final AwContentsClient awContentsClient,
            boolean supportsLegacyQuirks, final TestDependencyFactory testDependencyFactory) {
        return mTestCommon.createAwTestContainerView(
                awContentsClient, supportsLegacyQuirks, testDependencyFactory);
    }

    public AwBrowserContext getAwBrowserContext() {
        return mTestCommon.getAwBrowserContext();
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient) {
        return mTestCommon.createDetachedAwTestContainerView(awContentsClient);
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient, boolean supportsLegacyQuirks,
            TestDependencyFactory testDependencyFactory) {
        return mTestCommon.createDetachedAwTestContainerView(
                awContentsClient, supportsLegacyQuirks, testDependencyFactory);
    }

    protected boolean isHardwareAcceleratedTest() {
        return mTestCommon.isHardwareAcceleratedTest();
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(final AwContentsClient client)
            throws Exception {
        return mTestCommon.createAwTestContainerViewOnMainSync(client);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client, final boolean supportsLegacyQuirks) {
        return mTestCommon.createAwTestContainerViewOnMainSync(client, supportsLegacyQuirks);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(final AwContentsClient client,
            final boolean supportsLegacyQuirks, final TestDependencyFactory testDependencyFactory) {
        return mTestCommon.createAwTestContainerViewOnMainSync(
                client, supportsLegacyQuirks, testDependencyFactory);
    }

    public void destroyAwContentsOnMainSync(final AwContents awContents) {
        mTestCommon.destroyAwContentsOnMainSync(awContents);
    }

    public String getTitleOnUiThread(final AwContents awContents) throws Exception {
        return mTestCommon.getTitleOnUiThread(awContents);
    }

    public AwSettings getAwSettingsOnUiThread(final AwContents awContents) throws Exception {
        return mTestCommon.getAwSettingsOnUiThread(awContents);
    }

    /**
     * Verify double quotes in both sides of the raw string. Strip the double quotes and
     * returns rest of the string.
     */
    protected String maybeStripDoubleQuotes(String raw) {
        return mTestCommon.maybeStripDoubleQuotes(raw);
    }

    /**
     * Executes the given snippet of JavaScript code within the given ContentView. Returns the
     * result of its execution in JSON format.
     */
    public String executeJavaScriptAndWaitForResult(final AwContents awContents,
            TestAwContentsClient viewClient, final String code) throws Exception {
        return mTestCommon.executeJavaScriptAndWaitForResult(awContents, viewClient, code);
    }

    /**
     * Executes JavaScript code within the given ContentView to get the text content in
     * document body. Returns the result string without double quotes.
     */
    public String getJavaScriptResultBodyTextContent(
            final AwContents awContents, final TestAwContentsClient viewClient) throws Exception {
        return mTestCommon.getJavaScriptResultBodyTextContent(awContents, viewClient);
    }

    /**
     * Wrapper around CriteriaHelper.pollInstrumentationThread. This uses AwTestBase-specifc
     * timeouts and treats timeouts and exceptions as test failures automatically.
     */
    public static void pollInstrumentationThread(final Callable<Boolean> callable)
            throws Exception {
        AwTestCommon.pollInstrumentationThread(callable);
    }

    /**
     * Wrapper around {@link AwTestBase#poll()} but runs the callable on the UI thread.
     */
    public void pollUiThread(final Callable<Boolean> callable) throws Exception {
        mTestCommon.pollUiThread(callable);
    }

    /**
     * Clears the resource cache. Note that the cache is per-application, so this will clear the
     * cache for all WebViews used.
     */
    public void clearCacheOnUiThread(final AwContents awContents, final boolean includeDiskFiles)
            throws Exception {
        mTestCommon.clearCacheOnUiThread(awContents, includeDiskFiles);
    }

    /**
     * Returns pure page scale.
     */
    public float getScaleOnUiThread(final AwContents awContents) throws Exception {
        return mTestCommon.getScaleOnUiThread(awContents);
    }

    /**
     * Returns page scale multiplied by the screen density.
     */
    public float getPixelScaleOnUiThread(final AwContents awContents) throws Exception {
        return mTestCommon.getPixelScaleOnUiThread(awContents);
    }

    /**
     * Returns whether a user can zoom the page in.
     */
    public boolean canZoomInOnUiThread(final AwContents awContents) throws Exception {
        return mTestCommon.canZoomInOnUiThread(awContents);
    }

    /**
     * Returns whether a user can zoom the page out.
     */
    public boolean canZoomOutOnUiThread(final AwContents awContents) throws Exception {
        return mTestCommon.canZoomOutOnUiThread(awContents);
    }

    public void killRenderProcessOnUiThreadAsync(final AwContents awContents) throws Exception {
        mTestCommon.killRenderProcessOnUiThreadAsync(awContents);
    }

    /**
     * Loads the main html then triggers the popup window.
     */
    public void triggerPopup(final AwContents parentAwContents,
            TestAwContentsClient parentAwContentsClient, TestWebServer testWebServer,
            String mainHtml, String popupHtml, String popupPath, String triggerScript)
            throws Exception {
        mTestCommon.triggerPopup(parentAwContents, parentAwContentsClient, testWebServer, mainHtml,
                popupHtml, popupPath, triggerScript);
    }

    /**
     * Supplies the popup window with AwContents then waits for the popup window to finish loading.
     */
    public PopupInfo connectPendingPopup(final AwContents parentAwContents) throws Exception {
        return mTestCommon.connectPendingPopup(parentAwContents);
    }

    @Override
    public boolean testMethodHasAnnotation(Class<? extends Annotation> clazz) {
        return mCurrentTestDescription.getAnnotation(clazz) != null ? true : false;
    }

    @Override
    public Instrumentation getInstrumentation() {
        return InstrumentationRegistry.getInstrumentation();
    }
}
