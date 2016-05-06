// Copyright 2015 The Chromium Authors. All rights reserved.
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
import android.test.InstrumentationTestRunner;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.widget.ListView;

import org.chromium.base.PerfTraceEvent;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.PerfTest;
import org.chromium.base.test.util.parameter.BaseParameter;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.document.IncognitoDocumentActivity;
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
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.parameters.AddFakeAccountToAppParameter;
import org.chromium.chrome.test.util.parameters.AddFakeAccountToOsParameter;
import org.chromium.chrome.test.util.parameters.AddGoogleAccountToOsParameter;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.RenderProcessLimit;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.io.File;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Method;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Base class for all Chrome instrumentation tests.
 * All tests must inherit from this class and define their own test methods
 * See ChromeTabbedActivityTestBase.java for example.
 * @param <T> A {@link ChromeActivity} class
 */
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        // Preconnect causes issues with the single-threaded Java test server.
        ChromeSwitches.DISABLE_PRECONNECT})
public abstract class ChromeActivityTestCaseBase<T extends ChromeActivity>
        extends BaseActivityInstrumentationTestCase<T> {

    private static final String TAG = "ChromeActivityTestCaseBase";

    // The number of ms to wait for the rendering activity to be started.
    protected static final int ACTIVITY_START_TIMEOUT_MS = 1000;

    private static final String PERF_ANNOTATION_FORMAT = "**PERFANNOTATION(%s):";

    private static final String MEMORY_TRACE_GRAPH_SUFFIX = " - browser PSS";

    private static final String PERF_OUTPUT_FILE = "PerfTestData.txt";

    private static final long OMNIBOX_FIND_SUGGESTION_TIMEOUT_MS = 10 * 1000;

    public ChromeActivityTestCaseBase(Class<T> activityClass) {
        super(activityClass);
    }

    protected boolean mSkipClearAppData = false;

    private Thread.UncaughtExceptionHandler mDefaultUncaughtExceptionHandler;

    private class ChromeUncaughtExceptionHandler implements Thread.UncaughtExceptionHandler {
        @Override
        public void uncaughtException(Thread t, Throwable e) {
            String stackTrace = Log.getStackTraceString(e);
            if (e.getClass().getName().endsWith("StrictModeViolation")) {
                stackTrace += "\nSearch logcat for \"StrictMode policy violation\" for full stack.";
            }
            Bundle resultsBundle = new Bundle();
            resultsBundle.putString(InstrumentationTestRunner.REPORT_KEY_NAME_CLASS,
                    getClass().getName());
            resultsBundle.putString(InstrumentationTestRunner.REPORT_KEY_NAME_TEST, getName());
            resultsBundle.putString(InstrumentationTestRunner.REPORT_KEY_STACK, stackTrace);
            getInstrumentation().sendStatus(-1, resultsBundle);
            mDefaultUncaughtExceptionHandler.uncaughtException(t, e);
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mDefaultUncaughtExceptionHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new ChromeUncaughtExceptionHandler());
        ApplicationTestUtils.setUp(getInstrumentation().getTargetContext(), !mSkipClearAppData);
        setActivityInitialTouchMode(false);
        startMainActivity();
    }

    @Override
    protected void tearDown() throws Exception {
        ApplicationTestUtils.tearDown(getInstrumentation().getTargetContext());
        Thread.setDefaultUncaughtExceptionHandler(mDefaultUncaughtExceptionHandler);
        super.tearDown();
    }

    /**
     * Called to start the Main Activity, the subclass should implemented with it desired start
     * method.
     * TODO: Make startMainActivityFromLauncher the default.
     */
    public abstract void startMainActivity() throws InterruptedException;

    /**
     * Matches testString against baseString.
     * Returns 0 if there is no match, 1 if an exact match and 2 if a fuzzy match.
     */
    protected static int matchUrl(String baseString, String testString) {
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
    protected void startActivityCompletely(Intent intent) {
        final Class<?> activityClazz =
                FeatureUtilities.isDocumentMode(getInstrumentation().getTargetContext())
                ? DocumentActivity.class : ChromeTabbedActivity.class;
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                activityClazz.getName(), null, false);
        Activity activity = getInstrumentation().startActivitySync(intent);
        assertNotNull("Main activity did not start", activity);
        ChromeActivity chromeActivity = (ChromeActivity)
                monitor.waitForActivityWithTimeout(ACTIVITY_START_TIMEOUT_MS);
        assertNotNull("ChromeActivity did not start", chromeActivity);
        setActivity(chromeActivity);
        Log.d(TAG, "startActivityCompletely <<");
    }

    /** Convenience function for {@link ApplicationTestUtils#clearAppData(Context)}. */
    protected void clearAppData() throws Exception {
        ApplicationTestUtils.clearAppData(getInstrumentation().getTargetContext());
    }

    /**
     * Enables or disables network predictions, i.e. prerendering, prefetching, DNS preresolution,
     * etc. Network predictions are enabled by default.
     */
    protected void setNetworkPredictionEnabled(final boolean enabled) {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setNetworkPredictionEnabled(enabled);
            }
        });
    }

    /**
     * Starts (synchronously) a drag motion. Normally followed by dragTo() and dragEnd().
     *
     * @param x
     * @param y
     * @param downTime (in ms)
     * @see TestTouchUtils
     */
    protected void dragStart(float x, float y, long downTime) {
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
    protected void dragTo(float fromX, float toX, float fromY,
            float toY, int stepCount, long downTime) {
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
    protected void dragEnd(float x, float y, long downTime) {
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
        final Semaphore s = new Semaphore(0);
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... arg0) {
                s.release();
                return null;
            }
        }.execute();
        assertTrue(s.tryAcquire(timeout, TimeUnit.MILLISECONDS));
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     * @param url            The url to load in the current tab.
     * @param secondsToWait  The number of seconds to wait for the page to be loaded.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrl(final String url, long secondsToWait)
            throws IllegalArgumentException, InterruptedException {
        return loadUrlInTab(url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                getActivity().getActivityTab(), secondsToWait);
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     * @param url The url to load in the current tab.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrl(final String url) throws IllegalArgumentException, InterruptedException {
        return loadUrlInTab(url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                getActivity().getActivityTab());
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
    public int loadUrlInTab(final String url, final int pageTransition, final Tab tab,
            long secondsToWait) throws InterruptedException {
        assertNotNull("Cannot load the url in a null tab", tab);
        final AtomicInteger result = new AtomicInteger();

        ChromeTabUtils.waitForTabPageLoaded(tab, new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        result.set(tab.loadUrl(
                                new LoadUrlParams(url, pageTransition)));
                    }
                });
            }
        }, secondsToWait);
        getInstrumentation().waitForIdleSync();
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
    public int loadUrlInTab(final String url, final int pageTransition, final Tab tab)
            throws InterruptedException {
        return loadUrlInTab(url, pageTransition, tab, CallbackHelper.WAIT_TIMEOUT_SECONDS);
    }

    /**
     * Load a url in a new tab. The {@link Tab} will pretend to be created from a link.
     * @param url The url of the page to load.
     */
    public Tab loadUrlInNewTab(final String url) throws InterruptedException {
        return loadUrlInNewTab(url, false);
    }

    /**
     * Load a url in a new tab. The {@link Tab} will pretend to be created from a link.
     * @param url The url of the page to load.
     * @param incognito Whether the new tab should be incognito.
     */
    public Tab loadUrlInNewTab(final String url, final boolean incognito)
            throws InterruptedException {
        Tab tab = null;
        if (FeatureUtilities.isDocumentMode(getInstrumentation().getTargetContext())) {
            Runnable activityTrigger = new Runnable() {
                @Override
                public void run() {
                    ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                        @Override
                        public void run() {
                            AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(
                                    new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL));
                            ChromeLauncherActivity.launchDocumentInstance(
                                    getActivity(), incognito, asyncParams);
                        }
                    });
                }
            };
            final DocumentActivity activity = ActivityUtils.waitForActivity(
                    getInstrumentation(),
                    incognito ? IncognitoDocumentActivity.class : DocumentActivity.class,
                    activityTrigger);
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return activity.getActivityTab() != null;
                }
            });
            tab = activity.getActivityTab();
        } else {
            try {
                tab = ThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
                    @Override
                    public Tab call() throws Exception {
                        return getActivity().getTabCreator(incognito)
                                .launchUrl(url, TabLaunchType.FROM_LINK);
                    }
                });
            } catch (ExecutionException e) {
                fail("Failed to create new tab");
            }
        }
        ChromeTabUtils.waitForTabPageLoaded(tab, url);
        getInstrumentation().waitForIdleSync();
        return tab;
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
        Intent intent = new Intent(
                TextUtils.isEmpty(url) ? Intent.ACTION_MAIN : Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        startMainActivityFromIntent(intent, url);
    }

    /**
     * Starts the Main activity and open a blank page.
     * This is faster and less flakyness-prone than starting on the NTP.
     */
    protected void startMainActivityOnBlankPage() throws InterruptedException {
        startMainActivityWithURL("about:blank");
    }

    /**
     * Starts the Main activity as if it was started from an external application, on the specified
     * URL.
     */
    protected void startMainActivityFromExternalApp(String url, String appId)
            throws InterruptedException {
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
    protected void startMainActivityFromIntent(Intent intent, String url)
            throws InterruptedException {
        prepareUrlIntent(intent, url);

        final boolean isDocumentMode =
                FeatureUtilities.isDocumentMode(getInstrumentation().getTargetContext());

        startActivityCompletely(intent);

        CriteriaHelper.pollUiThread(new Criteria("Tab never selected/initialized.") {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab() != null;
            }
        });
        Tab tab = getActivity().getActivityTab();

        ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);

        if (!isDocumentMode && tab != null && NewTabPage.isNTPUrl(tab.getUrl())) {
            NewTabPageTestUtils.waitForNtpLoaded(tab);
        }

        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupComplete();
            }
        });

        assertNotNull(tab);
        assertNotNull(tab.getView());
        getInstrumentation().waitForIdleSync();
    }

    /**
     * Prepares a URL intent to start the activity.
     * @param intent the intent to be modified
     * @param url the URL to be used (may be null)
     */
    protected Intent prepareUrlIntent(Intent intent, String url) {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(getInstrumentation().getTargetContext(),
                ChromeLauncherActivity.class));

        if (url != null) {
            intent.setData(Uri.parse(url));
        }

        try {
            Method method = getClass().getMethod(getName(), (Class[]) null);
            if (((AnnotatedElement) method).isAnnotationPresent(RenderProcessLimit.class)) {
                RenderProcessLimit limit = method.getAnnotation(RenderProcessLimit.class);
                intent.putExtra(ChromeTabbedActivity.INTENT_EXTRA_TEST_RENDER_PROCESS_LIMIT,
                        limit.value());
            }
        } catch (Exception ex) {
            // Ignore exception.
        }
        return intent;
    }

    /**
     * Open an incognito tab by invoking the 'new incognito' menu item.
     * Returns when receiving the 'PAGE_LOAD_FINISHED' notification.
     */
    protected void newIncognitoTabFromMenu() throws InterruptedException {
        Tab tab = null;

        if (FeatureUtilities.isDocumentMode(getInstrumentation().getTargetContext())) {
            final IncognitoDocumentActivity activity = ActivityUtils.waitForActivity(
                    getInstrumentation(), IncognitoDocumentActivity.class,
                    new Runnable() {
                        @Override
                        public void run() {
                            MenuUtils.invokeCustomMenuActionSync(
                                    getInstrumentation(), getActivity(),
                                    R.id.new_incognito_tab_menu_id);
                        }
                    });

            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return activity.getActivityTab() != null;
                }
            });

            tab = activity.getActivityTab();
        } else {
            final CallbackHelper createdCallback = new CallbackHelper();
            final CallbackHelper selectedCallback = new CallbackHelper();

            TabModel incognitoTabModel = getActivity().getTabModelSelector().getModel(true);
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

            MenuUtils.invokeCustomMenuActionSync(getInstrumentation(), getActivity(),
                    R.id.new_incognito_tab_menu_id);

            try {
                createdCallback.waitForCallback(0);
            } catch (TimeoutException ex) {
                fail("Never received tab created event");
            }
            try {
                selectedCallback.waitForCallback(0);
            } catch (TimeoutException ex) {
                fail("Never received tab selected event");
            }
            incognitoTabModel.removeObserver(observer);

            tab = getActivity().getActivityTab();
        }

        ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        getInstrumentation().waitForIdleSync();
        Log.d(TAG, "newIncognitoTabFromMenu <<");
    }

    /**
     * New multiple incognito tabs by invoking the 'new incognito' menu item n times.
     * @param n The number of tabs you want to create.
     */
    protected void newIncognitoTabsFromMenu(int n)
            throws InterruptedException {
        while (n > 0) {
            newIncognitoTabFromMenu();
            --n;
        }
    }

    /**
     * @return The number of incognito tabs currently open.
     */
    protected int incognitoTabsCount() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() {
                TabModelSelector tabModelSelector;
                if (FeatureUtilities.isDocumentMode(getInstrumentation().getTargetContext())) {
                    tabModelSelector = ChromeApplication.getDocumentTabModelSelector();
                } else {
                    tabModelSelector = getActivity().getTabModelSelector();
                }
                return tabModelSelector.getModel(true).getCount();
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
    public void typeInOmnibox(final String text, final boolean oneCharAtATime)
            throws InterruptedException {
        final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        assertNotNull(urlBar);

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
            final Instrumentation instrumentation = getInstrumentation();
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
    protected OmniboxSuggestion findOmniboxSuggestion(String inputText, String displayText,
            String url, int type) throws InterruptedException {
        long endTime = System.currentTimeMillis() + OMNIBOX_FIND_SUGGESTION_TIMEOUT_MS;

        // Multiple suggestion events may occur before the one we're interested in is received.
        final CallbackHelper onSuggestionsReceivedHelper = new CallbackHelper();
        final LocationBarLayout locationBar =
                (LocationBarLayout) getActivity().findViewById(R.id.location_bar);
        locationBar.setAutocompleteController(new AutocompleteController(locationBar) {
            @Override
            public void onSuggestionsReceived(
                    List<OmniboxSuggestion> suggestions,
                    String inlineAutocompleteText,
                    long currentNativeAutocompleteResult) {
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
                    onSuggestionsReceivedHelper.waitForCallback(
                            callbackCount, 1,
                            endTime - System.currentTimeMillis(), TimeUnit.MILLISECONDS);
                } catch (TimeoutException exception) {
                    return null;
                }

                // Wait for suggestions to show up.
                CriteriaHelper.pollInstrumentationThread(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ((LocationBarLayout) getActivity().findViewById(
                                R.id.location_bar)).getSuggestionList() != null;
                    }
                }, 3000, 10);
                final ListView suggestionListView = locationBar.getSuggestionList();
                OmniboxResultItem popupItem = (OmniboxResultItem) suggestionListView
                        .getItemAtPosition(0);
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
    protected List<InfoBar> getInfoBars() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<InfoBar>>() {
            @Override
            public List<InfoBar> call() throws Exception {
                Tab currentTab = getActivity().getActivityTab();
                assertNotNull(currentTab);
                assertNotNull(currentTab.getInfoBarContainer());
                return currentTab.getInfoBarContainer().getInfoBarsForTesting();
            }
        });
    }

    /**
     * Launches the preferences menu and starts the preferences activity named fragmentName.
     * Returns the activity that was started.
     */
    protected Preferences startPreferences(String fragmentName) {
        Context context = getInstrumentation().getTargetContext();
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(context, fragmentName);
        Activity activity = getInstrumentation().startActivitySync(intent);
        assertTrue(activity instanceof Preferences);
        return (Preferences) activity;
    }

    /**
     * Executes the given snippet of JavaScript code within the current tab. Returns the result of
     * its execution in JSON format.
     * @throws InterruptedException
     */
    protected String runJavaScriptCodeInCurrentTab(String code) throws InterruptedException,
            TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getActivity().getCurrentContentViewCore().getWebContents(), code);
    }

    @Override
    protected void runTest() throws Throwable {
        String perfTagAnalysisString = setupPotentialPerfTest();
        super.runTest();
        endPerfTest(perfTagAnalysisString);
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
    protected void assertWaitForPageScaleFactorMatch(final float expectedScale)
            throws InterruptedException {
        ApplicationTestUtils.assertWaitForPageScaleFactorMatch(getActivity(), expectedScale);
    }

    /**
     * This method creates a special string that tells the python test harness what
     * trace calls to track for this particular test run.  It can support multiple trace calls for
     * each test and will make a new graph entry for all of them.  It should be noted that this
     * method eats all exceptions.  This is so that it can never be the cause of a test failure.
     * We still need to call this method even if we know the test will not run (ie: willTestRun is
     * false).  This is because this method lets the python test harness know not to expect any
     * perf output in this case.  In the case that the autoTrace parameter is set for the current
     * test method, this will also start the PerfTrace facility automatically.
     *
     * @param willTestRun Whether or not this test will actually be run.
     * @return A specially formatted string that contains which JSON perf markers to look at. This
     *         will be analyzed by the perf test harness.
     */
    @SuppressFBWarnings({
            "REC_CATCH_EXCEPTION",
            "RV_RETURN_VALUE_IGNORED_BAD_PRACTICE",
            })
    private String setupPotentialPerfTest() {
        File perfFile = getInstrumentation().getTargetContext().getFileStreamPath(
                PERF_OUTPUT_FILE);
        perfFile.delete();
        PerfTraceEvent.setOutputFile(perfFile);

        String perfAnnotationString = "";

        try {
            Method method = getClass().getMethod(getName(), (Class[]) null);
            PerfTest annotation = method.getAnnotation(PerfTest.class);
            if (annotation != null) {
                StringBuilder annotationData = new StringBuilder();
                annotationData.append(String.format(PERF_ANNOTATION_FORMAT, method.getName()));

                // Grab the minimum number of trace calls we will track (if names(),
                // graphNames(), and graphValues() do not have the same number of elements, we
                // will track as many as we can given the data available.
                final int maxIndex = Math.min(annotation.traceNames().length, Math.min(
                        annotation.graphNames().length, annotation.seriesNames().length));

                List<String> allNames = new LinkedList<String>();
                for (int i = 0; i < maxIndex; ++i) {
                    // Prune out all of ',' and ';' from the strings.  Replace them with '-'.
                    String name = annotation.traceNames()[i].replaceAll("[,;]", "-");
                    allNames.add(name);
                    String graphName = annotation.graphNames()[i].replaceAll("[,;]", "-");
                    String seriesName = annotation.seriesNames()[i].replaceAll("[,;]", "-");
                    if (annotation.traceTiming()) {
                        annotationData.append(name).append(",")
                                .append(graphName).append(",")
                                .append(seriesName).append(';');
                    }

                    // If memory tracing is enabled, add an additional graph for each one
                    // defined to track timing perf that will track the corresponding memory
                    // usage.
                    // Keep the series name the same, but just append a memory identifying
                    // prefix to the graph.
                    if (annotation.traceMemory()) {
                        String memName = PerfTraceEvent.makeMemoryTraceNameFromTimingName(name);
                        String memGraphName = PerfTraceEvent.makeSafeTraceName(
                                graphName, MEMORY_TRACE_GRAPH_SUFFIX);
                        annotationData.append(memName).append(",")
                                .append(memGraphName).append(",")
                                .append(seriesName).append(';');
                        allNames.add(memName);
                    }
                }
                // We only record perf trace events for the names explicitly listed.
                PerfTraceEvent.setFilter(allNames);

                // Figure out if we should automatically start or stop the trace.
                if (annotation.autoTrace()) {
                    PerfTraceEvent.setEnabled(true);
                }
                PerfTraceEvent.setTimingTrackingEnabled(annotation.traceTiming());
                PerfTraceEvent.setMemoryTrackingEnabled(annotation.traceMemory());

                perfAnnotationString = annotationData.toString();
            }
        } catch (Exception ex) {
            // Eat exception here.
        }

        return perfAnnotationString;
    }

    /**
     * This handles cleaning up the performance component of this test if it was a UI Perf test.
     * This includes potentially shutting down PerfTraceEvent.  This method eats all exceptions so
     * that it can never be the cause of a test failure.  The test harness will wait for
     * {@code perfTagAnalysisString} to show up in the logcat before processing the JSON perf file,
     * giving this method the chance to flush and dump the performance data before the harness reads
     * it.
     *
     * @param perfTagAnalysisString A specially formatted string that tells the perf test harness
     *                              which perf tags to analyze.
     */
    private void endPerfTest(String perfTagAnalysisString) {
        try {
            Method method = getClass().getMethod(getName(), (Class[]) null);
            PerfTest annotation = method.getAnnotation(PerfTest.class);
            if (annotation != null) {
                if (PerfTraceEvent.enabled()) {
                    PerfTraceEvent.setEnabled(false);
                }

                System.out.println(perfTagAnalysisString);
            }
        } catch (Exception ex) {
            // Eat exception here.
        }
    }
}
