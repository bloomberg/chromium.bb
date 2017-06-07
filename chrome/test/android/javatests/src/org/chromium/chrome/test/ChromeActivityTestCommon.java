// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.provider.Browser;
import android.support.test.internal.runner.listener.InstrumentationResultPrinter;
import android.text.TextUtils;
import android.widget.ListView;

import org.junit.Assert;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.AutocompleteController;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.OmniboxResultsAdapter.OmniboxResultItem;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.RenderProcessLimit;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Method;
import java.util.Calendar;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

final class ChromeActivityTestCommon<T extends ChromeActivity> {
    private static final String TAG = "cr_CATestCommon";

    // The number of ms to wait for the rendering activity to be started.
    static final int ACTIVITY_START_TIMEOUT_MS = 1000;

    private static final String PERF_ANNOTATION_FORMAT = "**PERFANNOTATION(%s):";

    private static final String MEMORY_TRACE_GRAPH_SUFFIX = " - browser PSS";

    private static final String PERF_OUTPUT_FILE = "PerfTestData.txt";

    private static final long OMNIBOX_FIND_SUGGESTION_TIMEOUT_MS = 10 * 1000;

    protected boolean mSkipClearAppData;
    private Thread.UncaughtExceptionHandler mDefaultUncaughtExceptionHandler;
    private Class<T> mChromeActivityClass;
    private ChromeTestCommonCallback<T> mCallback;

    ChromeActivityTestCommon(Class<T> activityClass, ChromeTestCommonCallback<T> callback) {
        mChromeActivityClass = activityClass;
        mCallback = callback;
    }

    private class ChromeUncaughtExceptionHandler implements Thread.UncaughtExceptionHandler {
        @Override
        public void uncaughtException(Thread t, Throwable e) {
            String stackTrace = android.util.Log.getStackTraceString(e);
            if (e.getClass().getName().endsWith("StrictModeViolation")) {
                stackTrace += "\nSearch logcat for \"StrictMode policy violation\" for full stack.";
            }
            Bundle resultsBundle = new Bundle();
            resultsBundle.putString(
                    InstrumentationResultPrinter.REPORT_KEY_NAME_CLASS, getClass().getName());
            resultsBundle.putString(
                    InstrumentationResultPrinter.REPORT_KEY_NAME_TEST, mCallback.getTestName());
            resultsBundle.putString(InstrumentationResultPrinter.REPORT_KEY_STACK, stackTrace);
            mCallback.getInstrumentation().sendStatus(-1, resultsBundle);
            mDefaultUncaughtExceptionHandler.uncaughtException(t, e);
        }
    }

    void setUp() throws Exception {
        mDefaultUncaughtExceptionHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new ChromeUncaughtExceptionHandler());
        ApplicationTestUtils.setUp(
                mCallback.getInstrumentation().getTargetContext(), !mSkipClearAppData);
        mCallback.setActivityInitialTouchMode(false);

        // Preload Calendar so that it does not trigger ReadFromDisk Strict mode violations if
        // called on the UI Thread. See https://crbug.com/705477 and https://crbug.com/577185
        Calendar.getInstance();
    }

    void tearDown() throws Exception {
        ApplicationTestUtils.tearDown(mCallback.getInstrumentation().getTargetContext());
        Thread.setDefaultUncaughtExceptionHandler(mDefaultUncaughtExceptionHandler);
    }

    /**
     * Matches testString against baseString.
     * Returns 0 if there is no match, 1 if an exact match and 2 if a fuzzy match.
     */
    static int matchUrl(String baseString, String testString) {
        if (baseString.equals(testString)) {
            return 1;
        }
        if (baseString.contains(testString)) {
            return 2;
        }
        return 0;
    }

    /**
     * Invokes {@link Instrumentation#startActivitySync(Intent)} and sets the
     * test case's activity to the result. See the documentation for
     * {@link Instrumentation#startActivitySync(Intent)} on the timing of the
     * return, but generally speaking the activity's "onCreate" has completed
     * and the activity's main looper has become idle.
     */
    void startActivityCompletely(Intent intent) {
        final CallbackHelper activityCallback = new CallbackHelper();
        final AtomicReference<T> activityRef = new AtomicReference<>();
        ActivityStateListener stateListener = new ActivityStateListener() {
            @SuppressWarnings("unchecked")
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (newState == ActivityState.RESUMED) {
                    if (!mChromeActivityClass.isAssignableFrom(activity.getClass())) {
                        return;
                    }

                    activityRef.set((T) activity);
                    activityCallback.notifyCalled();
                    ApplicationStatus.unregisterActivityStateListener(this);
                }
            }
        };
        ApplicationStatus.registerStateListenerForAllActivities(stateListener);

        try {
            mCallback.getInstrumentation().startActivitySync(intent);
            activityCallback.waitForCallback("Activity did not start as expected", 0);
            T activity = activityRef.get();
            Assert.assertNotNull("Activity reference is null.", activity);
            mCallback.setActivity(activity);
            Log.d(TAG, "startActivityCompletely <<");
        } catch (InterruptedException | TimeoutException e) {
            throw new RuntimeException(e);
        } finally {
            ApplicationStatus.unregisterActivityStateListener(stateListener);
        }
    }

    /** Convenience function for {@link ApplicationTestUtils#clearAppData(Context)}. */
    void clearAppData() {
        ApplicationTestUtils.clearAppData(mCallback.getInstrumentation().getTargetContext());
    }

    /**
     * Enables or disables network predictions, i.e. prerendering, prefetching, DNS preresolution,
     * etc. Network predictions are enabled by default.
     */
    void setNetworkPredictionEnabled(final boolean enabled) {
        mCallback.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setNetworkPredictionEnabled(enabled);
            }
        });
    }

    /**
     * Waits for {@link AsyncTask}'s that have been queued to finish. Note, this
     * only waits for tasks that have been started using the default
     * {@link java.util.concurrent.Executor}, which executes tasks serially.
     *
     * @param timeout how long to wait for tasks to complete
     */
    void waitForAsyncTasks(long timeout) throws InterruptedException {
        final Semaphore s = new Semaphore(0);
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... arg0) {
                s.release();
                return null;
            }
        }
                .execute();
        Assert.assertTrue(s.tryAcquire(timeout, TimeUnit.MILLISECONDS));
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     * @param url            The url to load in the current tab.
     * @param secondsToWait  The number of seconds to wait for the page to be loaded.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    int loadUrl(final String url, long secondsToWait)
            throws IllegalArgumentException, InterruptedException {
        return loadUrlInTab(url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mCallback.getActivity().getActivityTab(), secondsToWait);
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     * @param url The url to load in the current tab.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    int loadUrl(final String url) throws IllegalArgumentException, InterruptedException {
        return loadUrlInTab(url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                mCallback.getActivity().getActivityTab());
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
    int loadUrlInTab(final String url, final int pageTransition, final Tab tab, long secondsToWait)
            throws InterruptedException {
        Assert.assertNotNull("Cannot load the url in a null tab", tab);
        final AtomicInteger result = new AtomicInteger();

        ChromeTabUtils.waitForTabPageLoaded(tab, new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        result.set(tab.loadUrl(new LoadUrlParams(url, pageTransition)));
                    }
                });
            }
        }, secondsToWait);
        mCallback.getInstrumentation().waitForIdleSync();
        return result.get();
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
    int loadUrlInTab(final String url, final int pageTransition, final Tab tab)
            throws InterruptedException {
        return loadUrlInTab(url, pageTransition, tab, CallbackHelper.WAIT_TIMEOUT_SECONDS);
    }

    /**
     * Load a url in a new tab. The {@link Tab} will pretend to be created from a link.
     * @param url The url of the page to load.
     */
    Tab loadUrlInNewTab(final String url) throws InterruptedException {
        return loadUrlInNewTab(url, false);
    }

    /**
     * Load a url in a new tab. The {@link Tab} will pretend to be created from a link.
     * @param url The url of the page to load.
     * @param incognito Whether the new tab should be incognito.
     */
    Tab loadUrlInNewTab(final String url, final boolean incognito) throws InterruptedException {
        Tab tab = null;
        try {
            tab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
                @Override
                public Tab call() throws Exception {
                    return mCallback.getActivity().getTabCreator(incognito).launchUrl(
                            url, TabLaunchType.FROM_LINK);
                }
            });
        } catch (ExecutionException e) {
            Assert.fail("Failed to create new tab");
        }
        ChromeTabUtils.waitForTabPageLoaded(tab, url);
        mCallback.getInstrumentation().waitForIdleSync();
        return tab;
    }

    /**
     * Simulates starting Main Activity from launcher, blocks until it is started.
     */
    void startMainActivityFromLauncher() throws InterruptedException {
        startMainActivityWithURL(null);
    }

    /**
     * Starts the Main activity on the specified URL. Passing a null URL ensures the default page is
     * loaded, which is the NTP with a new profile .
     */
    void startMainActivityWithURL(String url) throws InterruptedException {
        // Only launch Chrome.
        Intent intent =
                new Intent(TextUtils.isEmpty(url) ? Intent.ACTION_MAIN : Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        startMainActivityFromIntent(intent, url);
    }

    /**
     * Starts the Main activity and open a blank page.
     * This is faster and less flakyness-prone than starting on the NTP.
     */
    void startMainActivityOnBlankPage() throws InterruptedException {
        startMainActivityWithURL("about:blank");
    }

    /**
     * Starts the Main activity as if it was started from an external application, on the specified
     * URL.
     */
    void startMainActivityFromExternalApp(String url, String appId) throws InterruptedException {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        if (appId != null) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, appId);
        }
        startMainActivityFromIntent(intent, url);
    }

    /**
     * Starts the Main activity using the passed intent, and using the specified URL.
     * This method waits for DEFERRED_STARTUP to fire as well as a subsequent
     * idle-sync of the main looper thread, and the initial tab must either
     * complete its load or it must crash before this method will return.
     */
    void startMainActivityFromIntent(Intent intent, String url) throws InterruptedException {
        prepareUrlIntent(intent, url);

        startActivityCompletely(intent);

        CriteriaHelper.pollUiThread(new Criteria("Tab never selected/initialized.") {
            @Override
            public boolean isSatisfied() {
                return mCallback.getActivity().getActivityTab() != null;
            }
        });
        Tab tab = mCallback.getActivity().getActivityTab();

        ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);

        if (tab != null && NewTabPage.isNTPUrl(tab.getUrl())) {
            NewTabPageTestUtils.waitForNtpLoaded(tab);
        }

        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp();
            }
        });

        Assert.assertNotNull(tab);
        Assert.assertNotNull(tab.getView());
        mCallback.getInstrumentation().waitForIdleSync();
    }

    /**
     * Prepares a URL intent to start the activity.
     * @param intent the intent to be modified
     * @param url the URL to be used (may be null)
     */
    Intent prepareUrlIntent(Intent intent, String url) {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(
                mCallback.getInstrumentation().getTargetContext(), ChromeLauncherActivity.class));

        if (url != null) {
            intent.setData(Uri.parse(url));
        }

        try {
            Method method = getClass().getMethod(mCallback.getTestName(), (Class[]) null);
            if (((AnnotatedElement) method).isAnnotationPresent(RenderProcessLimit.class)) {
                RenderProcessLimit limit = method.getAnnotation(RenderProcessLimit.class);
                intent.putExtra(
                        ChromeTabbedActivity.INTENT_EXTRA_TEST_RENDER_PROCESS_LIMIT, limit.value());
            }
        } catch (NoSuchMethodException ex) {
            // Ignore exception.
        }
        return intent;
    }

    /**
     * Open an incognito tab by invoking the 'new incognito' menu item.
     * Returns when receiving the 'PAGE_LOAD_FINISHED' notification.
     */
    void newIncognitoTabFromMenu() throws InterruptedException {
        Tab tab = null;

        final CallbackHelper createdCallback = new CallbackHelper();
        final CallbackHelper selectedCallback = new CallbackHelper();

        TabModel incognitoTabModel = mCallback.getActivity().getTabModelSelector().getModel(true);
        TabModelObserver observer = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, TabLaunchType type) {
                createdCallback.notifyCalled();
            }

            @Override
            public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
                selectedCallback.notifyCalled();
            }
        };
        incognitoTabModel.addObserver(observer);

        MenuUtils.invokeCustomMenuActionSync(mCallback.getInstrumentation(),
                mCallback.getActivity(), R.id.new_incognito_tab_menu_id);

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            Assert.fail("Never received tab created event");
        }
        try {
            selectedCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            Assert.fail("Never received tab selected event");
        }
        incognitoTabModel.removeObserver(observer);

        tab = mCallback.getActivity().getActivityTab();

        ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        mCallback.getInstrumentation().waitForIdleSync();
        Log.d(TAG, "newIncognitoTabFromMenu <<");
    }

    /**
     * New multiple incognito tabs by invoking the 'new incognito' menu item n times.
     * @param n The number of tabs you want to create.
     */
    void newIncognitoTabsFromMenu(int n) throws InterruptedException {
        while (n > 0) {
            newIncognitoTabFromMenu();
            --n;
        }
    }

    /**
     * @return The number of incognito tabs currently open.
     */
    int incognitoTabsCount() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() {
                return mCallback.getActivity().getTabModelSelector().getModel(true).getCount();
            }
        });
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
    void typeInOmnibox(final String text, final boolean oneCharAtATime)
            throws InterruptedException {
        final UrlBar urlBar = (UrlBar) mCallback.getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.requestFocus();
                if (!oneCharAtATime) {
                    urlBar.setText(text);
                }
            }
        });

        if (oneCharAtATime) {
            final Instrumentation instrumentation = mCallback.getInstrumentation();
            for (int i = 0; i < text.length(); ++i) {
                instrumentation.sendStringSync(text.substring(i, i + 1));
                // Let's put some delay between key strokes to simulate a user pressing the keys.
                Thread.sleep(20);
            }
        }
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
    OmniboxSuggestion findOmniboxSuggestion(String inputText, String displayText, String url,
            int type) throws InterruptedException {
        long endTime = System.currentTimeMillis() + OMNIBOX_FIND_SUGGESTION_TIMEOUT_MS;

        // Multiple suggestion events may occur before the one we're interested in is received.
        final CallbackHelper onSuggestionsReceivedHelper = new CallbackHelper();
        final LocationBarLayout locationBar =
                (LocationBarLayout) mCallback.getActivity().findViewById(R.id.location_bar);
        locationBar.setAutocompleteController(new AutocompleteController(locationBar) {
            @Override
            public void onSuggestionsReceived(List<OmniboxSuggestion> suggestions,
                    String inlineAutocompleteText, long currentNativeAutocompleteResult) {
                super.onSuggestionsReceived(
                        suggestions, inlineAutocompleteText, currentNativeAutocompleteResult);
                onSuggestionsReceivedHelper.notifyCalled();
            }
        });

        try {
            typeInOmnibox(inputText, false);

            while (true) {
                try {
                    int callbackCount = onSuggestionsReceivedHelper.getCallCount();
                    onSuggestionsReceivedHelper.waitForCallback(callbackCount, 1,
                            endTime - System.currentTimeMillis(), TimeUnit.MILLISECONDS);
                } catch (TimeoutException exception) {
                    return null;
                }

                // Wait for suggestions to show up.
                CriteriaHelper.pollInstrumentationThread(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ((LocationBarLayout) mCallback.getActivity().findViewById(
                                        R.id.location_bar))
                                       .getSuggestionList()
                                != null;
                    }
                }, 3000, 10);
                final ListView suggestionListView = locationBar.getSuggestionList();
                OmniboxResultItem popupItem =
                        (OmniboxResultItem) suggestionListView.getItemAtPosition(0);
                OmniboxSuggestion suggestion = popupItem.getSuggestion();
                if (suggestionListView.getCount() == 1
                        && suggestion.getDisplayText().equals(inputText)
                        && !suggestion.getDisplayText().equals(displayText)) {
                    // If there is only one suggestion and it's the same as inputText,
                    // wait for other suggestions before looking for the one we want.
                    CriteriaHelper.pollInstrumentationThread(new Criteria() {
                        @Override
                        public boolean isSatisfied() {
                            return suggestionListView.getCount() > 1;
                        }
                    }, 3000, 10);
                }
                int count = suggestionListView.getCount();
                for (int i = 0; i < count; i++) {
                    popupItem = (OmniboxResultItem) suggestionListView.getItemAtPosition(i);
                    suggestion = popupItem.getSuggestion();
                    if (suggestion.getType() != type) {
                        continue;
                    }
                    if (displayText != null && !suggestion.getDisplayText().equals(displayText)) {
                        continue;
                    }
                    if (url != null && !suggestion.getUrl().equals(url)) {
                        continue;
                    }
                    return suggestion;
                }
            }
        } finally {
            locationBar.setAutocompleteController(new AutocompleteController(locationBar));
        }
    }

    /**
     * Returns the infobars being displayed by the current tab, or null if they don't exist.
     */
    List<InfoBar> getInfoBars() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<InfoBar>>() {
            @Override
            public List<InfoBar> call() throws Exception {
                Tab currentTab = mCallback.getActivity().getActivityTab();
                Assert.assertNotNull(currentTab);
                Assert.assertNotNull(currentTab.getInfoBarContainer());
                return currentTab.getInfoBarContainer().getInfoBarsForTesting();
            }
        });
    }

    /**
     * Launches the preferences menu and starts the preferences activity named fragmentName.
     * Returns the activity that was started.
     */
    Preferences startPreferences(String fragmentName) {
        Context context = mCallback.getInstrumentation().getTargetContext();
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(context, fragmentName);
        Activity activity = mCallback.getInstrumentation().startActivitySync(intent);
        Assert.assertTrue(activity instanceof Preferences);
        return (Preferences) activity;
    }

    /**
     * Executes the given snippet of JavaScript code within the current tab. Returns the result of
     * its execution in JSON format.
     * @throws InterruptedException
     */
    String runJavaScriptCodeInCurrentTab(String code)
            throws InterruptedException, TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mCallback.getActivity().getCurrentContentViewCore().getWebContents(), code);
    }

    /**
     * Waits till the ContentViewCore receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    void assertWaitForPageScaleFactorMatch(final float expectedScale) {
        ApplicationTestUtils.assertWaitForPageScaleFactorMatch(
                mCallback.getActivity(), expectedScale);
    }

    public interface ChromeTestCommonCallback<T extends ChromeActivity> {
        String getTestName();
        void setActivity(T t);
        Instrumentation getInstrumentation();
        void setActivityInitialTouchMode(boolean touchMode);
        T getActivity();
    }
}
