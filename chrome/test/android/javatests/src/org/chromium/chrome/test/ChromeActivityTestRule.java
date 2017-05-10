// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestion;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCommon.ChromeTestCommonCallback;
import org.chromium.chrome.test.util.ApplicationTestUtils;

import java.util.List;
import java.util.concurrent.TimeoutException;

//TODO(yolandyan): break this test rule down to smaller rules once the junit4 migration is over
public class ChromeActivityTestRule<T extends ChromeActivity>
        extends ActivityTestRule<T> implements ChromeTestCommonCallback<T> {
    private final ChromeActivityTestCommon<T> mTestCommon;
    private String mCurrentTestName;

    public static final String DISABLE_NETWORK_PREDICTION_FLAG =
            "--disable-features=NetworkPrediction";

    // ChromeActivityTestRule
    private T mSetActivity;

    /**
     * @param activityClass
     */
    public ChromeActivityTestRule(Class<T> activityClass) {
        this(activityClass, false);
    }

    public ChromeActivityTestRule(Class<T> activityClass, boolean initialTouchMode) {
        super(activityClass, initialTouchMode, false);
        mTestCommon = new ChromeActivityTestCommon<T>(activityClass, this);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        mCurrentTestName = description.getMethodName();
        final Statement superBase = super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                mTestCommon.setUp();
                base.evaluate();
            }
        }, description);
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                String perfTagAnalysisString = mTestCommon.setupPotentialPerfTest();
                superBase.evaluate();
                mTestCommon.endPerfTest(perfTagAnalysisString);
            }
        };
    }

    /**
     * Return the timeout limit for Chrome activty start in tests
     */
    public static int getActivityStartTimeoutMs() {
        return ChromeActivityTestCommon.ACTIVITY_START_TIMEOUT_MS;
    }

    @Override
    protected void afterActivityFinished() {
        try {
            mTestCommon.tearDown();
        } catch (Exception e) {
            throw new RuntimeException("Failed to tearDown", e);
        }
        super.afterActivityFinished();
    }

    // TODO(yolandyan): remove this once startActivityCompletely is refactored out of
    // ChromeActivityTestRule
    @Override
    public T getActivity() {
        if (mSetActivity != null) {
            return mSetActivity;
        }
        return super.getActivity();
    }

    /**
     * Matches testString against baseString.
     * Returns 0 if there is no match, 1 if an exact match and 2 if a fuzzy match.
     */
    public static int matchUrl(String baseString, String testString) {
        return ChromeActivityTestCommon.matchUrl(baseString, testString);
    }

    /**
     * Invokes {@link Instrumentation#startActivitySync(Intent)} and sets the
     * test case's activity to the result. See the documentation for
     * {@link Instrumentation#startActivitySync(Intent)} on the timing of the
     * return, but generally speaking the activity's "onCreate" has completed
     * and the activity's main looper has become idle.
     *
     * TODO(yolandyan): very similar to ActivityTestRule#launchActivity(Intent),
     * yet small differences remains (e.g. launchActivity() uses FLAG_ACTIVITY_NEW_TASK while
     * startActivityCompletely doesn't), need to refactor and use only launchActivity
     * after the JUnit4 migration
     */
    public void startActivityCompletely(Intent intent) {
        mTestCommon.startActivityCompletely(intent);
    }

    /** Convenience function for {@link ApplicationTestUtils#clearAppData(Context)}. */
    public void clearAppData() {
        mTestCommon.clearAppData();
    }

    /**
     * Enables or disables network predictions, i.e. prerendering, prefetching, DNS preresolution,
     * etc. Network predictions are enabled by default.
     */
    public void setNetworkPredictionEnabled(final boolean enabled) {
        mTestCommon.setNetworkPredictionEnabled(enabled);
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
    public void startMainActivityFromLauncher() throws InterruptedException {
        startMainActivityWithURL(null);
    }

    /**
     * Starts the Main activity on the specified URL. Passing a null URL ensures the default page is
     * loaded, which is the NTP with a new profile .
     */
    public void startMainActivityWithURL(String url) throws InterruptedException {
        // Only launch Chrome.
        mTestCommon.startMainActivityWithURL(url);
    }

    /**
     * Starts the Main activity and open a blank page.
     * This is faster and less flakyness-prone than starting on the NTP.
     */
    public void startMainActivityOnBlankPage() throws InterruptedException {
        mTestCommon.startMainActivityOnBlankPage();
    }

    /**
     * Starts the Main activity as if it was started from an external application, on the specified
     * URL.
     */
    public void startMainActivityFromExternalApp(String url, String appId)
            throws InterruptedException {
        mTestCommon.startMainActivityFromExternalApp(url, appId);
    }

    /**
     * Starts the Main activity using the passed intent, and using the specified URL.
     * This method waits for DEFERRED_STARTUP to fire as well as a subsequent
     * idle-sync of the main looper thread, and the initial tab must either
     * complete its load or it must crash before this method will return.
     */
    public void startMainActivityFromIntent(Intent intent, String url) throws InterruptedException {
        mTestCommon.startMainActivityFromIntent(intent, url);
    }

    /**
     * Prepares a URL intent to start the activity.
     * @param intent the intent to be modified
     * @param url the URL to be used (may be null)
     */
    public Intent prepareUrlIntent(Intent intent, String url) {
        return mTestCommon.prepareUrlIntent(intent, url);
    }

    /**
     * Open an incognito tab by invoking the 'new incognito' menu item.
     * Returns when receiving the 'PAGE_LOAD_FINISHED' notification.
     *
     * TODO(yolandyan): split this into the seperate test rule, this only applies to tabbed mode
     */
    public void newIncognitoTabFromMenu() throws InterruptedException {
        mTestCommon.newIncognitoTabFromMenu();
    }

    /**
     * New multiple incognito tabs by invoking the 'new incognito' menu item n times.
     * @param n The number of tabs you want to create.
     *
     * TODO(yolandyan): split this into the seperate test rule, this only applies to tabbed mode
     */
    public void newIncognitoTabsFromMenu(int n) throws InterruptedException {
        mTestCommon.newIncognitoTabsFromMenu(n);
    }

    /**
     * @return The number of incognito tabs currently open.
     */
    public int incognitoTabsCount() {
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
     *
     * TODO(yolandyan): split this into the seperate test rule, this only applies to tabbed mode
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
    public OmniboxSuggestion findOmniboxSuggestion(String inputText, String displayText, String url,
            int type) throws InterruptedException {
        return mTestCommon.findOmniboxSuggestion(inputText, displayText, url, type);
    }

    /**
     * Returns the infobars being displayed by the current tab, or null if they don't exist.
     */
    public List<InfoBar> getInfoBars() {
        return mTestCommon.getInfoBars();
    }

    /**
     * Launches the preferences menu and starts the preferences activity named fragmentName.
     * Returns the activity that was started.
     */
    public Preferences startPreferences(String fragmentName) {
        return mTestCommon.startPreferences(fragmentName);
    }

    /**
     * Executes the given snippet of JavaScript code within the current tab. Returns the result of
     * its execution in JSON format.
     * @throws InterruptedException
     */
    public String runJavaScriptCodeInCurrentTab(String code)
            throws InterruptedException, TimeoutException {
        return mTestCommon.runJavaScriptCodeInCurrentTab(code);
    }

    /**
     * Waits till the ContentViewCore receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    public void assertWaitForPageScaleFactorMatch(float expectedScale) {
        mTestCommon.assertWaitForPageScaleFactorMatch(expectedScale);
    }

    public String getName() {
        return mCurrentTestName;
    }

    @Override
    public String getTestName() {
        return mCurrentTestName;
    }

    @Override
    public Instrumentation getInstrumentation() {
        return InstrumentationRegistry.getInstrumentation();
    }

    /**
     * ActitivityTestRule already set initial touch mode in constructor.
     */
    @Override
    public void setActivityInitialTouchMode(boolean touchMode) {}

    @Override
    public void setActivity(T chromeActivity) {
        mSetActivity = chromeActivity;
    }
}
