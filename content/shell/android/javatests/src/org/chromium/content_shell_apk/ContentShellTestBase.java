// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.app.Instrumentation;
import android.content.Intent;

import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell_apk.ContentShellActivityTestRule.RerunWithUpdatedContainerView;
import org.chromium.content_shell_apk.ContentShellTestCommon.TestCommonCallback;

import java.lang.reflect.AnnotatedElement;
import java.util.concurrent.ExecutionException;

/**
 * Base test class for all ContentShell based tests.
 */
@CommandLineFlags.Add(ContentSwitches.ENABLE_TEST_INTENTS)
public class ContentShellTestBase extends BaseActivityInstrumentationTestCase<ContentShellActivity>
        implements TestCommonCallback<ContentShellActivity> {
    protected static final long WAIT_PAGE_LOADING_TIMEOUT_SECONDS =
            ContentShellTestCommon.WAIT_PAGE_LOADING_TIMEOUT_SECONDS;

    private ContentShellTestCommon mDelegate;

    public ContentShellTestBase() {
        super(ContentShellActivity.class);
        mDelegate = new ContentShellTestCommon(this);
    }

    @Override
    @SuppressWarnings("deprecation")
    protected void setUp() throws Exception {
        super.setUp();
        mDelegate.assertScreenIsOn();
    }

    /**
     * Starts the ContentShell activity and loads the given URL.
     * The URL can be null, in which case will default to ContentShellActivity.DEFAULT_SHELL_URL.
     */
    public ContentShellActivity launchContentShellWithUrl(String url) {
        return mDelegate.launchContentShellWithUrl(url);
    }

    // TODO(cjhopman): These functions are inconsistent with launchContentShell***. Should be
    // startContentShell*** and should use the url exactly without the getTestFileUrl call. Possibly
    // these two ways of starting the activity (launch* and start*) should be merged into one.
    /**
     * Starts the content shell activity with the provided test url.
     * The url is synchronously loaded.
     * @param url Test url to load.
     */
    public void startActivityWithTestUrl(String url) {
        mDelegate.launchContentShellWithUrlSync(url);
    }

    /**
     * Returns the current ContentViewCore or null if there is no ContentView.
     */
    public ContentViewCore getContentViewCore() {
        return mDelegate.getContentViewCore();
    }

    /**
     * Returns the WebContents of this Shell.
     */
    public WebContents getWebContents() {
        return mDelegate.getWebContents();
    }

    /**
     * Waits for the Active shell to finish loading.  This times out after
     * WAIT_FOR_ACTIVE_SHELL_LOADING_TIMEOUT milliseconds and it shouldn't be used for long
     * loading pages. Instead it should be used more for test initialization. The proper way
     * to wait is to use a TestCallbackHelperContainer after the initial load is completed.
     */
    public void waitForActiveShellToBeDoneLoading() {
        mDelegate.waitForActiveShellToBeDoneLoading();
    }

    /**
     * Creates a new {@link Shell} and waits for it to finish loading.
     * @param url The URL to create the new {@link Shell} with.
     * @return A new instance of a {@link Shell}.
     * @throws ExecutionException
     */
    public Shell loadNewShell(String url) throws ExecutionException {
        return mDelegate.loadNewShell(url);
    }

    /**
     * Loads a URL in the specified content view.
     *
     * @param navigationController The navigation controller to load the URL in.
     * @param callbackHelperContainer The callback helper container used to monitor progress.
     * @param params The URL params to use.
     */
    public void loadUrl(NavigationController navigationController,
            TestCallbackHelperContainer callbackHelperContainer, LoadUrlParams params)
            throws Throwable {
        mDelegate.loadUrl(navigationController, callbackHelperContainer, params);
    }

    /**
     * Handles performing an action on the UI thread that will return when the specified callback
     * is incremented.
     *
     * @param callbackHelper The callback helper that will be blocked on.
     * @param action The action to be performed on the UI thread.
     */
    public void handleBlockingCallbackAction(CallbackHelper callbackHelper, Runnable action)
            throws Throwable {
        mDelegate.handleBlockingCallbackAction(callbackHelper, action);
    }

    // TODO(aelias): This method needs to be removed once http://crbug.com/179511 is fixed.
    // Meanwhile, we have to wait if the page has the <meta viewport> tag.
    /**
     * Waits till the ContentViewCore receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    public void assertWaitForPageScaleFactorMatch(float expectedScale) {
        mDelegate.assertWaitForPageScaleFactorMatch(expectedScale);
    }

    /**
     * Replaces the {@link ContentViewCore#mContainerView} with a newly created
     * {@link ContentView}.
     */
    @SuppressWarnings("javadoc")
    public void replaceContainerView() throws Throwable {
        mDelegate.replaceContainerView();
    }

    @SuppressWarnings("deprecation")
    @Override
    protected void runTest() throws Throwable {
        super.runTest();
        try {
            AnnotatedElement method = getClass().getMethod(getName(), (Class[]) null);
            if (method.isAnnotationPresent(RerunWithUpdatedContainerView.class)) {
                replaceContainerView();
                super.runTest();
            }
        } catch (Throwable e) {
            throw new Throwable("@RerunWithUpdatedContainerView failed."
                    + " See ContentShellTestBase#runTest.", e);
        }
    }

    @SuppressWarnings("deprecation")
    @Override
    public ContentShellActivity getActivityForTestCommon() {
        return getActivity();
    }

    @Override
    @SuppressWarnings("deprecation")
    public Instrumentation getInstrumentationForTestCommon() {
        return getInstrumentation();
    }

    @SuppressWarnings("deprecation")
    @Override
    public ContentShellActivity launchActivityWithIntentForTestCommon(Intent intent) {
        setActivityIntent(intent);
        return getActivity();
    }

    @SuppressWarnings("deprecation")
    @Override
    public void runOnUiThreadForTestCommon(Runnable runnable) throws Throwable {
        runTestOnUiThread(runnable);
    }

    @Override
    public ContentViewCore getContentViewCoreForTestCommon() {
        return getContentViewCore();
    }

    @Override
    public WebContents getWebContentsForTestCommon() {
        return getWebContents();
    }

    @Override
    public void waitForActiveShellToBeDoneLoadingForTestCommon() {
        waitForActiveShellToBeDoneLoading();
    }

    @Override
    public ContentShellActivity launchContentShellWithUrlForTestCommon(String url) {
        return launchContentShellWithUrl(url);
    }
}
