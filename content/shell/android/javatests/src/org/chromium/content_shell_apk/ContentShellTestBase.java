// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_shell.Shell;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Method;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Base test class for all ContentShell based tests.
 */
public class ContentShellTestBase extends ActivityInstrumentationTestCase2<ContentShellActivity> {

    /** The maximum time the waitForActiveShellToBeDoneLoading method will wait. */
    private static final long WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT = scaleTimeout(10000);

    protected static final long WAIT_PAGE_LOADING_TIMEOUT_SECONDS = scaleTimeout(15);

    public ContentShellTestBase() {
        super(ContentShellActivity.class);
    }

    /**
     * Starts the ContentShell activity and loads the given URL.
     * The URL can be null, in which case will default to ContentShellActivity.DEFAULT_SHELL_URL.
     */
    protected ContentShellActivity launchContentShellWithUrl(String url) {
        return launchContentShellWithUrlAndCommandLineArgs(url, null);
    }

    /**
     * Starts the ContentShell activity appending the provided command line arguments
     * and loads the given URL. The URL can be null, in which case will default to
     * ContentShellActivity.DEFAULT_SHELL_URL.
     */
    protected ContentShellActivity launchContentShellWithUrlAndCommandLineArgs(String url,
            String[] commandLineArgs) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (url != null) intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(getInstrumentation().getTargetContext(),
              ContentShellActivity.class));
        if (commandLineArgs != null) {
            intent.putExtra(ContentShellActivity.COMMAND_LINE_ARGS_KEY, commandLineArgs);
        }
        setActivityIntent(intent);
        return getActivity();
    }

    // TODO(cjhopman): These functions are inconsistent with launchContentShell***. Should be
    // startContentShell*** and should use the url exactly without the getTestFileUrl call. Possibly
    // these two ways of starting the activity (launch* and start*) should be merged into one.
    /**
     * Starts the content shell activity with the provided test url.
     * The url is synchronously loaded.
     * @param url Test url to load.
     */
    protected void startActivityWithTestUrl(String url) throws Throwable {
        launchContentShellWithUrl(UrlUtils.getTestFileUrl(url));
        assertNotNull(getActivity());
        assertTrue(waitForActiveShellToBeDoneLoading());
        assertEquals(UrlUtils.getTestFileUrl(url), getContentViewCore().getWebContents().getUrl());
    }

    /**
     * Starts the content shell activity with the provided test url and optional command line
     * arguments to append.
     * The url is synchronously loaded.
     * @param url Test url to load.
     * @param commandLineArgs Optional command line args to append when launching the activity.
     */
    protected void startActivityWithTestUrlAndCommandLineArgs(
            String url, String[] commandLineArgs) throws Throwable {
        launchContentShellWithUrlAndCommandLineArgs(
                UrlUtils.getTestFileUrl(url), commandLineArgs);
        assertNotNull(getActivity());
        assertTrue(waitForActiveShellToBeDoneLoading());
    }

    /**
     * Returns the current ContentViewCore or null if there is no ContentView.
     */
    protected ContentViewCore getContentViewCore() {
        return getActivity().getActiveShell().getContentViewCore();
    }

    /**
     * Waits for the Active shell to finish loading.  This times out after
     * WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT milliseconds and it shouldn't be used for long
     * loading pages. Instead it should be used more for test initialization. The proper way
     * to wait is to use a TestCallbackHelperContainer after the initial load is completed.
     * @return Whether or not the Shell was actually finished loading.
     * @throws InterruptedException
     */
    protected boolean waitForActiveShellToBeDoneLoading() throws InterruptedException {
        final ContentShellActivity activity = getActivity();

        // Wait for the Content Shell to be initialized.
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    final AtomicBoolean isLoaded = new AtomicBoolean(false);
                    runTestOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            Shell shell = activity.getActiveShell();
                            if (shell != null) {
                                // There are two cases here that need to be accounted for.
                                // The first is that we've just created a Shell and it isn't
                                // loading because it has no URL set yet.  The second is that
                                // we've set a URL and it actually is loading.
                                isLoaded.set(!shell.isLoading()
                                        && !TextUtils.isEmpty(shell.getContentViewCore()
                                                .getWebContents().getUrl()));
                            } else {
                                isLoaded.set(false);
                            }
                        }
                    });

                    return isLoaded.get();
                } catch (Throwable e) {
                    return false;
                }
            }
        }, WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Loads a URL in the specified content view.
     *
     * @param viewCore The content view core to load the URL in.
     * @param callbackHelperContainer The callback helper container used to monitor progress.
     * @param params The URL params to use.
     */
    protected void loadUrl(
            final ContentViewCore viewCore, TestCallbackHelperContainer callbackHelperContainer,
            final LoadUrlParams params) throws Throwable {
        handleBlockingCallbackAction(
                callbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        viewCore.getWebContents().getNavigationController().loadUrl(params);
                    }
                });
    }

    /**
     * Handles performing an action on the UI thread that will return when the specified callback
     * is incremented.
     *
     * @param callbackHelper The callback helper that will be blocked on.
     * @param action The action to be performed on the UI thread.
     */
    protected void handleBlockingCallbackAction(
            CallbackHelper callbackHelper, Runnable action) throws Throwable {
        int currentCallCount = callbackHelper.getCallCount();
        runTestOnUiThread(action);
        callbackHelper.waitForCallback(
                currentCallCount, 1, WAIT_PAGE_LOADING_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    // TODO(aelias): This method needs to be removed once http://crbug.com/179511 is fixed.
    // Meanwhile, we have to wait if the page has the <meta viewport> tag.
    /**
     * Waits till the ContentViewCore receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    protected void assertWaitForPageScaleFactorMatch(final float expectedScale)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getContentViewCore().getScale() == expectedScale;
            }
        }));
    }

    /**
     * Replaces the {@link ContentViewCore#mContainerView} with a newly created
     * {@link ContentView}.
     */
    @SuppressWarnings("javadoc")
    protected void replaceContainerView() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
            public void run() {
                ContentView cv = ContentView.newInstance(getActivity(), getContentViewCore());
                ((ViewGroup) getContentViewCore().getContainerView().getParent()).addView(cv);
                getContentViewCore().setContainerView(cv);
                getContentViewCore().setContainerViewInternals(cv);
                cv.requestFocus();
            }
        });
    }

    @Override
    protected void runTest() throws Throwable {
        super.runTest();
        try {
            Method method = getClass().getMethod(getName(), (Class[]) null);
            if (method.isAnnotationPresent(RerunWithUpdatedContainerView.class)) {
                replaceContainerView();
                super.runTest();
            }
        } catch (Throwable e) {
            throw new Throwable("@RerunWithUpdatedContainerView failed."
                    + " See ContentShellTestBase#runTest.", e);
        }
    }

    /**
     * Annotation for tests that should be executed a second time after replacing
     * the ContentViewCore's container view (see {@link #runTest()}).
     *
     * <p>Please note that {@link #setUp()} is only invoked once before both runs,
     * and that any state changes produced by the first run are visible to the second run.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface RerunWithUpdatedContainerView {
    }
}
