// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.view.View;

import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.parameter.BaseParameter;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestion;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCommon.ChromeTestCommonCallback;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.parameters.AddFakeAccountToAppParameter;
import org.chromium.chrome.test.util.parameters.AddFakeAccountToOsParameter;
import org.chromium.chrome.test.util.parameters.AddGoogleAccountToOsParameter;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Base class for all Chrome instrumentation tests.
 * All tests must inherit from this class and define their own test methods
 * See ChromeTabbedActivityTestBase.java for example.
 * @param <T> A {@link ChromeActivity} class
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        // Preconnect causes issues with the single-threaded Java test server.
        "--disable-features=NetworkPrediction"})
public abstract class ChromeActivityTestCaseBase<T extends ChromeActivity>
        extends BaseActivityInstrumentationTestCase<T> implements ChromeTestCommonCallback<T> {
    private final ChromeActivityTestCommon<T> mTestCommon;

    public ChromeActivityTestCaseBase(Class<T> activityClass) {
        super(activityClass);
        mTestCommon = new ChromeActivityTestCommon<>(activityClass, this);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestCommon.setUp();
        startMainActivity();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestCommon.tearDown();
        super.tearDown();
    }

    /**
     * Called to start the Main Activity, the subclass should implemented with it desired start
     * method.
     * TODO: Make startMainActivityFromLauncher the default.
     */
    public abstract void startMainActivity() throws InterruptedException;

    /**
     * Return the timeout limit for Chrome activty start in tests
     */
    protected static int getActivityStartTimeoutMs() {
        return ChromeActivityTestCommon.ACTIVITY_START_TIMEOUT_MS;
    }

    /**
     * Matches testString against baseString.
     * Returns 0 if there is no match, 1 if an exact match and 2 if a fuzzy match.
     */
    protected static int matchUrl(String baseString, String testString) {
        return ChromeActivityTestCommon.matchUrl(baseString, testString);
    }

    /**
     * Invokes {@link Instrumentation#startActivitySync(Intent)} and sets the
     * test case's activity to the result. See the documentation for
     * {@link Instrumentation#startActivitySync(Intent)} on the timing of the
     * return, but generally speaking the activity's "onCreate" has completed
     * and the activity's main looper has become idle.
     */
    protected void startActivityCompletely(Intent intent) {
        mTestCommon.startActivityCompletely(intent);
    }

    /** Convenience function for {@link ApplicationTestUtils#clearAppData(Context)}. */
    protected void clearAppData() {
        mTestCommon.clearAppData();
    }

    /**
     * Enables or disables network predictions, i.e. prerendering, prefetching, DNS preresolution,
     * etc. Network predictions are enabled by default.
     */
    protected void setNetworkPredictionEnabled(final boolean enabled) {
        mTestCommon.setNetworkPredictionEnabled(enabled);
    }

    /**
     * Starts (synchronously) a drag motion. Normally followed by dragTo() and dragEnd().
     *
     * @param x
     * @param y
     * @param downTime (in ms)
     * @see TestTouchUtils
     */
    public void dragStart(float x, float y, long downTime) {
        TouchCommon.dragStart(getActivity(), x, y, downTime);
    }

    /**
     * Drags / moves (synchronously) to the specified coordinates. Normally preceeded by
     * dragStart() and followed by dragEnd()
     *
     * @param fromX
     * @param toX
     * @param fromY
     * @param toY
     * @param stepCount
     * @param downTime (in ms)
     * @see TestTouchUtils
     */
    public void dragTo(
            float fromX, float toX, float fromY, float toY, int stepCount, long downTime) {
        TouchCommon.dragTo(getActivity(), fromX, toX, fromY, toY, stepCount, downTime);
    }

    /**
     * Finishes (synchronously) a drag / move at the specified coordinate.
     * Normally preceeded by dragStart() and dragTo().
     *
     * @param x
     * @param y
     * @param downTime (in ms)
     * @see TestTouchUtils
     */
    public void dragEnd(float x, float y, long downTime) {
        TouchCommon.dragEnd(getActivity(), x, y, downTime);
    }

    /**
     * Sends (synchronously) a single click to the center of the View.
     *
     * <p>
     * Differs from
     * {@link TestTouchUtils#singleClickView(android.app.Instrumentation, View)}
     * as this does not rely on injecting events into the different activity.  Injecting events has
     * been unreliable for us and simulating the touch events in this manner is just as effective.
     *
     * @param v The view to be clicked.
     */
    public void singleClickView(View v) {
        TouchCommon.singleClickView(v);
    }

    /**
     * Waits for {@link AsyncTask}'s that have been queued to finish. Note, this
     * only waits for tasks that have been started using the default
     * {@link java.util.concurrent.Executor}, which executes tasks serially.
     *
     * @param timeout how long to wait for tasks to complete
     */
    public void waitForAsyncTasks(long timeout) throws InterruptedException {
        mTestCommon.waitForAsyncTasks(timeout);
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     * @param url            The url to load in the current tab.
     * @param secondsToWait  The number of seconds to wait for the page to be loaded.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrl(String url, long secondsToWait)
            throws IllegalArgumentException, InterruptedException {
        return mTestCommon.loadUrl(url, secondsToWait);
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     * @param url The url to load in the current tab.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrl(String url) throws IllegalArgumentException, InterruptedException {
        return mTestCommon.loadUrl(url);
    }

    /**
     * @param url            The url of the page to load.
     * @param pageTransition The type of transition. see
     *                       {@link org.chromium.ui.base.PageTransition}
     *                       for valid values.
     * @param tab            The tab to load the url into.
     * @param secondsToWait  The number of seconds to wait for the page to be loaded.
     * @return               FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the
     *                       page has been prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrlInTab(String url, int pageTransition, Tab tab, long secondsToWait)
            throws InterruptedException {
        return mTestCommon.loadUrlInTab(url, pageTransition, tab, secondsToWait);
    }

    /**
     * @param url            The url of the page to load.
     * @param pageTransition The type of transition. see
     *                       {@link org.chromium.ui.base.PageTransition}
     *                       for valid values.
     * @param tab            The tab to load the url into.
     * @return               FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the
     *                       page has been prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrlInTab(String url, int pageTransition, Tab tab) throws InterruptedException {
        return mTestCommon.loadUrlInTab(url, pageTransition, tab);
    }

    /**
     * Load a url in a new tab. The {@link Tab} will pretend to be created from a link.
     * @param url The url of the page to load.
     */
    public Tab loadUrlInNewTab(String url) throws InterruptedException {
        return mTestCommon.loadUrlInNewTab(url);
    }

    /**
     * Load a url in a new tab. The {@link Tab} will pretend to be created from a link.
     * @param url The url of the page to load.
     * @param incognito Whether the new tab should be incognito.
     */
    public Tab loadUrlInNewTab(final String url, final boolean incognito)
            throws InterruptedException {
        return mTestCommon.loadUrlInNewTab(url, incognito);
    }

    /**
     * Simulates starting Main Activity from launcher, blocks until it is started.
     */
    protected void startMainActivityFromLauncher() throws InterruptedException {
        startMainActivityWithURL(null);
    }

    /**
     * Starts the Main activity on the specified URL. Passing a null URL ensures the default page is
     * loaded, which is the NTP with a new profile .
     */
    protected void startMainActivityWithURL(String url) throws InterruptedException {
        // Only launch Chrome.
        mTestCommon.startMainActivityWithURL(url);
    }

    /**
     * Starts the Main activity and open a blank page.
     * This is faster and less flakyness-prone than starting on the NTP.
     */
    protected void startMainActivityOnBlankPage() throws InterruptedException {
        mTestCommon.startMainActivityOnBlankPage();
    }

    /**
     * Starts the Main activity as if it was started from an external application, on the specified
     * URL.
     */
    protected void startMainActivityFromExternalApp(String url, String appId)
            throws InterruptedException {
        mTestCommon.startMainActivityFromExternalApp(url, appId);
    }

    /**
     * Starts the Main activity using the passed intent, and using the specified URL.
     * This method waits for DEFERRED_STARTUP to fire as well as a subsequent
     * idle-sync of the main looper thread, and the initial tab must either
     * complete its load or it must crash before this method will return.
     */
    protected void startMainActivityFromIntent(Intent intent, String url)
            throws InterruptedException {
        mTestCommon.startMainActivityFromIntent(intent, url);
    }

    /**
     * Prepares a URL intent to start the activity.
     * @param intent the intent to be modified
     * @param url the URL to be used (may be null)
     */
    protected Intent prepareUrlIntent(Intent intent, String url) {
        return mTestCommon.prepareUrlIntent(intent, url);
    }

    /**
     * Open an incognito tab by invoking the 'new incognito' menu item.
     * Returns when receiving the 'PAGE_LOAD_FINISHED' notification.
     */
    protected void newIncognitoTabFromMenu() throws InterruptedException {
        mTestCommon.newIncognitoTabFromMenu();
    }

    /**
     * New multiple incognito tabs by invoking the 'new incognito' menu item n times.
     * @param n The number of tabs you want to create.
     */
    protected void newIncognitoTabsFromMenu(int n)
            throws InterruptedException {
        mTestCommon.newIncognitoTabsFromMenu(n);
    }

    /**
     * @return The number of incognito tabs currently open.
     */
    protected int incognitoTabsCount() {
        return mTestCommon.incognitoTabsCount();
    }

    /**
     * Looks up the Omnibox in the view hierarchy and types the specified
     * text into it, requesting focus and using an inter-character delay of
     * 200ms.
     *
     * @param oneCharAtATime Whether to type text one character at a time or all at once.
     *
     * @throws InterruptedException
     */
    public void typeInOmnibox(String text, boolean oneCharAtATime) throws InterruptedException {
        mTestCommon.typeInOmnibox(text, oneCharAtATime);
    }

    /**
     * Searches for a given suggestion after typing given text in the Omnibox.
     *
     * @param inputText Input text to type into the Omnibox.
     * @param displayText Suggestion text expected to be found. Passing in null ignores this field.
     * @param url URL expected to be found. Passing in null ignores this field.
     * @param type Type of suggestion expected to be found. Passing in null ignores this field.
     *
     * @throws InterruptedException
     */
    protected OmniboxSuggestion findOmniboxSuggestion(String inputText, String displayText,
            String url, int type) throws InterruptedException {
        return mTestCommon.findOmniboxSuggestion(inputText, displayText, url, type);
    }

    /**
     * Returns the infobars being displayed by the current tab, or null if they don't exist.
     */
    protected List<InfoBar> getInfoBars() {
        return mTestCommon.getInfoBars();
    }

    /**
     * Launches the preferences menu and starts the preferences activity named fragmentName.
     * Returns the activity that was started.
     */
    protected Preferences startPreferences(String fragmentName) {
        return mTestCommon.startPreferences(fragmentName);
    }

    /**
     * Executes the given snippet of JavaScript code within the current tab. Returns the result of
     * its execution in JSON format.
     * @throws InterruptedException
     */
    protected String runJavaScriptCodeInCurrentTab(String code) throws InterruptedException,
            TimeoutException {
        return mTestCommon.runJavaScriptCodeInCurrentTab(code);
    }

    @Override
    protected Map<String, BaseParameter> createAvailableParameters() {
        Map<String, BaseParameter> availableParameters = super.createAvailableParameters();
        availableParameters.put(AddFakeAccountToAppParameter.PARAMETER_TAG,
                new AddFakeAccountToAppParameter(getParameterReader(), getInstrumentation()));
        availableParameters.put(AddFakeAccountToOsParameter.PARAMETER_TAG,
                new AddFakeAccountToOsParameter(getParameterReader(), getInstrumentation()));
        availableParameters.put(AddGoogleAccountToOsParameter.PARAMETER_TAG,
                new AddGoogleAccountToOsParameter(getParameterReader(), getInstrumentation()));
        return availableParameters;
    }

    /**
     * Waits till the ContentViewCore receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    protected void assertWaitForPageScaleFactorMatch(float expectedScale) {
        mTestCommon.assertWaitForPageScaleFactorMatch(expectedScale);
    }

    @Override
    public String getTestName() {
        return getName();
    }

    @Override
    public void setActivity(T t) {
        super.setActivity(t);
    }
}
