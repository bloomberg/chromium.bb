// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.PowerManager;
import android.text.TextUtils;
import android.view.ViewGroup;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell.ShellViewAndroidDelegate.OnCursorUpdateHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

/**
 * Implementation of utility methods for ContentShellTestBase and ContentShellActivityTestRule to
 * wrap around during instrumentation test JUnit3 to JUnit4 migration
 *
 * Please do not use this class' methods in places other than {@link ContentShellTestBase}
 * and {@link ContentShellActivityTestRule}
 */
public final class ContentShellTestCommon {
    /** The maximum time the waitForActiveShellToBeDoneLoading method will wait. */
    private static final long WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT = scaleTimeout(10000);
    static final long WAIT_PAGE_LOADING_TIMEOUT_SECONDS = scaleTimeout(15);

    private final TestCommonCallback<ContentShellActivity> mCallback;

    ContentShellTestCommon(TestCommonCallback<ContentShellActivity> callback) {
        mCallback = callback;
    }

    @TargetApi(Build.VERSION_CODES.KITKAT_WATCH)
    @SuppressWarnings("deprecation")
    void assertScreenIsOn() {
        PowerManager pm = (PowerManager) mCallback.getInstrumentationForTestCommon()
                                  .getContext()
                                  .getSystemService(Context.POWER_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
            Assert.assertTrue("Many tests will fail if the screen is not on.", pm.isInteractive());
        } else {
            Assert.assertTrue("Many tests will fail if the screen is not on.", pm.isScreenOn());
        }
    }

    ContentShellActivity launchContentShellWithUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (url != null) intent.setData(Uri.parse(url));
        intent.setComponent(
                new ComponentName(mCallback.getInstrumentationForTestCommon().getTargetContext(),
                        ContentShellActivity.class));
        return mCallback.launchActivityWithIntentForTestCommon(intent);
    }

    // TODO(yolandyan): This should use the url exactly without the getIsolatedTestFileUrl call.
    ContentShellActivity launchContentShellWithUrlSync(String url) {
        String isolatedTestFileUrl = UrlUtils.getIsolatedTestFileUrl(url);
        ContentShellActivity activity = launchContentShellWithUrl(isolatedTestFileUrl);
        Assert.assertNotNull(mCallback.getActivityForTestCommon());
        waitForActiveShellToBeDoneLoading();
        Assert.assertEquals(
                isolatedTestFileUrl, getContentViewCore().getWebContents().getLastCommittedUrl());
        return activity;
    }

    void waitForActiveShellToBeDoneLoading() {
        // Wait for the Content Shell to be initialized.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Shell shell = mCallback.getActivityForTestCommon().getActiveShell();
                // There are two cases here that need to be accounted for.
                // The first is that we've just created a Shell and it isn't
                // loading because it has no URL set yet.  The second is that
                // we've set a URL and it actually is loading.
                if (shell == null) {
                    updateFailureReason("Shell is null.");
                    return false;
                }
                if (shell.isLoading()) {
                    updateFailureReason("Shell is still loading.");
                    return false;
                }
                if (TextUtils.isEmpty(
                            shell.getContentViewCore().getWebContents().getLastCommittedUrl())) {
                    updateFailureReason("Shell's URL is empty or null.");
                    return false;
                }
                return true;
            }
        }, WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    OnCursorUpdateHelper getOnCursorUpdateHelper() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<OnCursorUpdateHelper>() {
            @Override
            public OnCursorUpdateHelper call() {
                return mCallback.getActivityForTestCommon()
                        .getActiveShell()
                        .getViewAndroidDelegate()
                        .getOnCursorUpdateHelper();
            }
        });
    }

    ContentViewCore getContentViewCore() {
        try {
            return ThreadUtils.runOnUiThreadBlocking(() -> {
                return mCallback.getActivityForTestCommon().getActiveShell().getContentViewCore();
            });
        } catch (ExecutionException e) {
            return null;
        }
    }

    WebContents getWebContents() {
        try {
            return ThreadUtils.runOnUiThreadBlocking(() -> {
                return mCallback.getActivityForTestCommon().getActiveShell().getWebContents();
            });
        } catch (ExecutionException e) {
            return null;
        }
    }

    RenderCoordinates getRenderCoordinates() {
        try {
            return ThreadUtils.runOnUiThreadBlocking(
                    () -> { return ((WebContentsImpl) getWebContents()).getRenderCoordinates(); });
        } catch (ExecutionException e) {
            return null;
        }
    }

    void loadUrl(final NavigationController navigationController,
            TestCallbackHelperContainer callbackHelperContainer, final LoadUrlParams params)
            throws Throwable {
        handleBlockingCallbackAction(
                callbackHelperContainer.getOnPageFinishedHelper(), new Runnable() {
                    @Override
                    public void run() {
                        navigationController.loadUrl(params);
                    }
                });
    }

    Shell loadNewShell(final String url) throws ExecutionException {
        Shell shell = ThreadUtils.runOnUiThreadBlocking(new Callable<Shell>() {
            @Override
            public Shell call() {
                mCallback.getActivityForTestCommon().getShellManager().launchShell(url);
                return mCallback.getActivityForTestCommon().getActiveShell();
            }
        });
        Assert.assertNotNull("Unable to create shell.", shell);
        Assert.assertEquals("Active shell unexpected.", shell,
                mCallback.getActivityForTestCommon().getActiveShell());
        waitForActiveShellToBeDoneLoading();
        return shell;
    }

    void handleBlockingCallbackAction(CallbackHelper callbackHelper, Runnable uiThreadAction)
            throws Throwable {
        int currentCallCount = callbackHelper.getCallCount();
        mCallback.runOnUiThreadForTestCommon(uiThreadAction);
        callbackHelper.waitForCallback(
                currentCallCount, 1, WAIT_PAGE_LOADING_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    void assertWaitForPageScaleFactorMatch(float expectedScale) {
        final RenderCoordinates coord = getRenderCoordinates();
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(expectedScale, new Callable<Float>() {
                    @Override
                    public Float call() {
                        return coord.getPageScaleFactor();
                    }
                }));
    }

    void replaceContainerView() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ContentView cv = ContentView.createContentView(
                        mCallback.getActivityForTestCommon(), getContentViewCore());
                ((ViewGroup) getContentViewCore().getContainerView().getParent()).addView(cv);
                getContentViewCore().setContainerView(cv);
                getContentViewCore().setContainerViewInternals(cv);
                cv.requestFocus();
            }
        });
    }

    /**
     * Interface used by TestRule and TestBase class to implement methods for TestCommonCallback
     * class to use.
     */
    public static interface TestCommonCallback<T extends Activity> {
        Instrumentation getInstrumentationForTestCommon();
        T launchActivityWithIntentForTestCommon(Intent t);
        T getActivityForTestCommon();
        void runOnUiThreadForTestCommon(Runnable runnable) throws Throwable;
        ContentViewCore getContentViewCoreForTestCommon();
        ContentShellActivity launchContentShellWithUrlForTestCommon(String url);
        WebContents getWebContentsForTestCommon();
        void waitForActiveShellToBeDoneLoadingForTestCommon();
    }
}
