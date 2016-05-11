// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.Instrumentation;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Build;
import android.util.Log;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwSwitches;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.parameter.Parameter;
import org.chromium.base.test.util.parameter.ParameterizedTest;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.util.TestWebServer;

import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Method;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * A base class for android_webview tests. WebView only runs on KitKat and later,
 * so make sure no one attempts to run the tests on earlier OS releases.
 *
 * By default, all tests run both in single-process mode, and with sandboxed renderer.
 * If a test doesn't yet work with sandboxed renderer, an entire class, or an individual test
 * method can be marked for single-process testing only by adding the following annotation:
 *
 * @ParameterizedTest.Set
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
@ParameterizedTest.Set(tests = {
            @ParameterizedTest(parameters = {
                        @Parameter(
                                tag = CommandLineFlags.Parameter.PARAMETER_TAG)}),
            @ParameterizedTest(parameters = {
                        @Parameter(
                                tag = CommandLineFlags.Parameter.PARAMETER_TAG,
                                arguments = {
                                    @Parameter.Argument(
                                        name = CommandLineFlags.Parameter.ADD_ARG,
                                        stringArray = {AwSwitches.WEBVIEW_SANDBOXED_RENDERER})
            })})})
public class AwTestBase
        extends BaseActivityInstrumentationTestCase<AwTestRunnerActivity> {
    public static final long WAIT_TIMEOUT_MS = scaleTimeout(15000);
    public static final int CHECK_INTERVAL = 100;
    private static final String TAG = "AwTestBase";

    // The browser context needs to be a process-wide singleton.
    private AwBrowserContext mBrowserContext;

    public AwTestBase() {
        super(AwTestRunnerActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mBrowserContext = new AwBrowserContext(new InMemorySharedPreferences(), appContext);

        super.setUp();
        if (needsBrowserProcessStarted()) {
            startBrowserProcess();
        }
    }

    protected void startBrowserProcess() throws Exception {
        final Context context = getActivity();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                AwBrowserProcess.start(context);
            }
        });
    }

    /**
     * Override this to return false if the test doesn't want the browser startup sequence to
     * be run automatically.
     * @return Whether the instrumentation test requires the browser process to already be started.
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
                awContents.loadUrl(url, extraHeaders);
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
                awContents.postUrl(url, mPostData);
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
                awContents.loadData(data, mimeType, isBase64Encoded ? "base64" : null);
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
                awContents.loadDataWithBaseURL(
                        baseUrl, data, mimeType, isBase64Encoded ? "base64" : null, historyUrl);
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
     * Stops loading on the UI thread.
     */
    public void stopLoading(final AwContents awContents) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                awContents.stopLoading();
            }
        });
    }

    public void waitForVisualStateCallback(final AwContents awContents) throws Exception {
        final CallbackHelper ch = new CallbackHelper();
        final int chCount = ch.getCallCount();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                final long requestId = 666;
                awContents.insertVisualStateCallback(requestId,
                        new AwContents.VisualStateCallback() {
                            @Override
                            public void onComplete(long id) {
                                assertEquals(requestId, id);
                                ch.notifyCalled();
                            }
                        });
            }
        });
        ch.waitForCallback(chCount);
    }

    // Waits for the pixel at the center of AwContents to color up into expectedColor.
    // Note that this is a stricter condition that waiting for a visual state callback,
    // as visual state callback only indicates that *something* has appeared in WebView.
    public void waitForPixelColorAtCenterOfView(final AwContents awContents,
            final AwTestContainerView testContainerView, final int expectedColor) throws Exception {
        pollUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                Bitmap bitmap = GraphicsTestUtils.drawAwContents(awContents, 2, 2,
                        -(float) testContainerView.getWidth() / 2,
                        -(float) testContainerView.getHeight() / 2);
                return bitmap.getPixel(0, 0) == expectedColor;
            }
        });
    }

    /**
     * Checks the current test has |clazz| annotation. Note this swallows NoSuchMethodException
     * and returns false in that case.
     */
    private boolean testMethodHasAnnotation(Class<? extends Annotation> clazz) {
        String testName = getName();
        Method method = null;
        try {
            method = getClass().getMethod(testName);
        } catch (NoSuchMethodException e) {
            Log.w(TAG, "Test method name not found.", e);
            return false;
        }

        // Cast to AnnotatedElement to work around a compilation failure.
        // Method.isAnnotationPresent() was removed in Java 8 (which is used by the Android N SDK),
        // so compilation with Java 7 fails. See crbug.com/608792.
        return ((AnnotatedElement) method).isAnnotationPresent(clazz);
    }

    /**
     * Factory class used in creation of test AwContents instances.
     *
     * Test cases can provide subclass instances to the createAwTest* methods in order to create an
     * AwContents instance with injected test dependencies.
     */
    public static class TestDependencyFactory extends AwContents.DependencyFactory {
        public AwTestContainerView createAwTestContainerView(AwTestRunnerActivity activity,
                boolean allowHardwareAcceleration) {
            return new AwTestContainerView(activity, allowHardwareAcceleration);
        }
        public AwSettings createAwSettings(Context context, boolean supportsLegacyQuirks) {
            return new AwSettings(context, false /* isAccessFromFileURLsGrantedByDefault */,
                    supportsLegacyQuirks, false /* allowEmptyDocumentPersistence */,
                    true /* allowGeolocationOnInsecureOrigins */);
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

    public AwBrowserContext getAwBrowserContext() {
        return mBrowserContext;
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient) {
        return createDetachedAwTestContainerView(awContentsClient, false);
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient, boolean supportsLegacyQuirks) {
        final TestDependencyFactory testDependencyFactory = createTestDependencyFactory();

        boolean allowHardwareAcceleration = isHardwareAcceleratedTest();
        final AwTestContainerView testContainerView =
                testDependencyFactory.createAwTestContainerView(getActivity(),
                        allowHardwareAcceleration);

        AwSettings awSettings = testDependencyFactory.createAwSettings(getActivity(),
                supportsLegacyQuirks);
        testContainerView.initialize(new AwContents(mBrowserContext, testContainerView,
                testContainerView.getContext(), testContainerView.getInternalAccessDelegate(),
                testContainerView.getNativeDrawGLFunctorFactory(), awContentsClient, awSettings,
                testDependencyFactory));
        return testContainerView;
    }

    protected boolean isHardwareAcceleratedTest() {
        return !testMethodHasAnnotation(DisableHardwareAccelerationForTest.class);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client) throws Exception {
        return createAwTestContainerViewOnMainSync(client, false);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client, final boolean supportsLegacyQuirks) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<AwTestContainerView>() {
            @Override
            public AwTestContainerView call() {
                return createAwTestContainerView(client, supportsLegacyQuirks);
            }
        });
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
     * Wrapper around CriteriaHelper.pollInstrumentationThread. This uses AwTestBase-specifc
     * timeouts and treats timeouts and exceptions as test failures automatically.
     */
    public static void pollInstrumentationThread(final Callable<Boolean> callable)
            throws Exception {
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

    /**
     * Wrapper around {@link AwTestBase#poll()} but runs the callable on the UI thread.
     */
    public void pollUiThread(final Callable<Boolean> callable) throws Exception {
        pollInstrumentationThread(new Callable<Boolean>() {
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

    /**
     * Loads the main html then triggers the popup window.
     */
    public void triggerPopup(final AwContents parentAwContents,
            TestAwContentsClient parentAwContentsClient, TestWebServer testWebServer,
            String mainHtml, String popupHtml, String popupPath, String triggerScript)
            throws Exception {
        enableJavaScriptOnUiThread(parentAwContents);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                parentAwContents.getSettings().setSupportMultipleWindows(true);
                parentAwContents.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
            }
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

    /**
     * POD object for holding references to helper objects of a popup window.
     */
    public static class PopupInfo {
        public final TestAwContentsClient popupContentsClient;
        public final AwTestContainerView popupContainerView;
        public final AwContents popupContents;
        public PopupInfo(TestAwContentsClient popupContentsClient,
                AwTestContainerView popupContainerView, AwContents popupContents) {
            this.popupContentsClient = popupContentsClient;
            this.popupContainerView = popupContainerView;
            this.popupContents = popupContents;
        }
    }

    /**
     * Supplies the popup window with AwContents then waits for the popup window to finish loading.
     */
    public PopupInfo connectPendingPopup(final AwContents parentAwContents) throws Exception {
        TestAwContentsClient popupContentsClient;
        AwTestContainerView popupContainerView;
        final AwContents popupContents;
        popupContentsClient = new TestAwContentsClient();
        popupContainerView = createAwTestContainerViewOnMainSync(popupContentsClient);
        popupContents = popupContainerView.getAwContents();
        enableJavaScriptOnUiThread(popupContents);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                parentAwContents.supplyContentsForPopup(popupContents);
            }
        });

        OnPageFinishedHelper onPageFinishedHelper = popupContentsClient.getOnPageFinishedHelper();
        int callCount = onPageFinishedHelper.getCallCount();
        onPageFinishedHelper.waitForCallback(callCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        return new PopupInfo(popupContentsClient, popupContainerView, popupContents);
    }
}
