// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.Instrumentation;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell.ShellViewAndroidDelegate.OnCursorUpdateHelper;
import org.chromium.content_shell_apk.ContentShellTestCommon.TestCommonCallback;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.concurrent.ExecutionException;

/**
 * ActivityTestRule for ContentShellActivity.
 *
 * Test can use this ActivityTestRule to launch or get ContentShellActivity.
 */
public class ContentShellActivityTestRule extends ActivityTestRule<ContentShellActivity>
        implements TestCommonCallback<ContentShellActivity> {
    private final ContentShellTestCommon mDelegate;
    private final boolean mLaunchActivity;

    protected static final long WAIT_PAGE_LOADING_TIMEOUT_SECONDS = scaleTimeout(15);

    public ContentShellActivityTestRule() {
        this(false, false);
    }

    public ContentShellActivityTestRule(boolean initialTouchMode, boolean launchActivity) {
        super(ContentShellActivity.class, initialTouchMode, launchActivity);
        mLaunchActivity = launchActivity;
        mDelegate = new ContentShellTestCommon(this);
    }

    @Override
    protected void beforeActivityLaunched() {
        mDelegate.assertScreenIsOn();
    }

    /**
     * Starts the ContentShell activity and loads the given URL.
     * The URL can be null, in which case will default to ContentShellActivity.DEFAULT_SHELL_URL.
     */
    public ContentShellActivity launchContentShellWithUrl(String url) {
        Assert.assertFalse(mLaunchActivity);
        return mDelegate.launchContentShellWithUrl(url);
    }

    /**
     * Starts the content shell activity with the provided test url.
     * The url is synchronously loaded.
     * @param url Test url to load.
     */
    public ContentShellActivity launchContentShellWithUrlSync(String url) {
        Assert.assertFalse(mLaunchActivity);
        return mDelegate.launchContentShellWithUrlSync(url);
    }

    /**
     * Returns the OnCursorUpdateHelper.
     */
    public OnCursorUpdateHelper getOnCursorUpdateHelper() throws ExecutionException {
        return mDelegate.getOnCursorUpdateHelper();
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
     * Returns the RenderCoordinates of the WebContents.
     */
    public RenderCoordinates getRenderCoordinates() {
        return mDelegate.getRenderCoordinates();
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
    public void replaceContainerView() throws Throwable {
        mDelegate.replaceContainerView();
    }

    /**
     * Annotation for tests that should be executed a second time after replacing
     * the ContentViewCore's container view.
     * <p>Please note that activity launch is only invoked once before both runs,
     * and that any state changes produced by the first run are visible to the second run.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface RerunWithUpdatedContainerView {}

    @Override
    public ContentShellActivity getActivityForTestCommon() {
        return getActivity();
    }

    @Override
    public Instrumentation getInstrumentationForTestCommon() {
        return InstrumentationRegistry.getInstrumentation();
    }

    @Override
    public ContentShellActivity launchActivityWithIntentForTestCommon(Intent t) {
        return launchActivity(t);
    }

    @Override
    public void runOnUiThreadForTestCommon(Runnable runnable) throws Throwable {
        runOnUiThread(runnable);
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
