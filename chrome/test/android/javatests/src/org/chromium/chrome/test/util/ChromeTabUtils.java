// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Instrumentation;
import android.text.TextUtils;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A utility class that contains methods generic to all Tabs tests.
 */
public class ChromeTabUtils {
    private static final String TAG = "cr_ChromeTabUtils";

    /**
     * An observer that waits for a Tab to load a page.
     *
     * The observer can be configured to either wait for the Tab to load a specific page
     * (if expectedUrl is non-null) or any page (otherwise). On seeing the tab finish
     * a page load or crash, the observer will notify the provided callback and stop
     * watching the tab. On load stop, the observer will decrement the provided latch
     * and continue watching the page in case the tab subsequently crashes or finishes
     * a page load.
     *
     * This may seem complicated, but it's intended to handle three distinct cases:
     *  1) Successful page load + observer starts watching before onPageLoadFinished fires.
     *     This is the most normal case: onPageLoadFinished fires, then onLoadStopped fires,
     *     and we see both.
     *  2) Crash on page load. onLoadStopped fires, then onCrash fires, and we see both.
     *  3) Successful page load + observer starts watching after onPageLoadFinished fires.
     *     We miss the onPageLoadFinished and *only* see onLoadStopped.
     *
     * Receiving onPageLoadFinished is sufficient to know that we're dealing with scenario #1.
     * Receiving onCrash is sufficient to know that we're dealing with scenario #2.
     * Receiving onLoadStopped without a preceding onPageLoadFinished indicates that we're dealing
     * with either scenario #2 *or* #3, so we have to keep watching for a call to onCrash.
     */
    private static class TabPageLoadedObserver extends EmptyTabObserver {
        private CallbackHelper mCallback;
        private String mExpectedUrl;
        private CountDownLatch mLoadStoppedLatch;

        public TabPageLoadedObserver(CallbackHelper loadCompleteCallback, String expectedUrl,
                CountDownLatch loadStoppedLatch) {
            mCallback = loadCompleteCallback;
            mExpectedUrl = expectedUrl;
            mLoadStoppedLatch = loadStoppedLatch;
        }

        @Override
        public void onCrash(Tab tab, boolean sadTabShown) {
            mCallback.notifyFailed("Tab crashed :(");
            tab.removeObserver(this);
        }

        @Override
        public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
            mLoadStoppedLatch.countDown();
        }

        @Override
        public void onPageLoadFinished(Tab tab) {
            if (mExpectedUrl == null || TextUtils.equals(tab.getUrl(), mExpectedUrl)) {
                mCallback.notifyCalled();
                tab.removeObserver(this);
            }
        }
    }

    private static boolean loadComplete(Tab tab, String url) {
        return !tab.isLoading() && (url == null || TextUtils.equals(tab.getUrl(), url))
                && !tab.getWebContents().isLoadingToDifferentDocument();
    }

    /**
     * Waits for the given tab to finish loading its current page.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param url The URL that will be waited to load for.  Pass in null if loading the
     *            current page is sufficient.
     */
    public static void waitForTabPageLoaded(final Tab tab, final String url)
            throws InterruptedException {
        Assert.assertFalse(ThreadUtils.runningOnUiThread());

        final CountDownLatch loadStoppedLatch = new CountDownLatch(1);
        final CallbackHelper loadedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (loadComplete(tab, url)) {
                    loadedCallback.notifyCalled();
                    return;
                }
                tab.addObserver(new TabPageLoadedObserver(loadedCallback, url, loadStoppedLatch));
            }
        });

        try {
            loadedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            // In the event that:
            //  1) the tab is on the correct page
            //  2) we weren't notified that the page load finished
            //  3) we *were* notified that the tab stopped loading
            //  4) the tab didn't crash
            //
            // then it's likely the case that we started observing the tab after
            // onPageLoadFinished but before onLoadStopped. (The latter sets tab.mIsLoading to
            // false.) Try to carry on with the test.
            if (loadStoppedLatch.getCount() == 0 && loadComplete(tab, url)) {
                Log.w(TAG,
                        "onPageLoadFinished was never called, but loading stopped "
                                + "on the expected page. Tentatively continuing.");
            } else {
                Assert.fail("Page did not load.  Tab information at time of failure --"
                        + " url: " + url + ", final URL: " + tab.getUrl() + ", load progress: "
                        + tab.getProgress() + ", is loading: " + Boolean.toString(tab.isLoading())
                        + ", web contents loading: "
                        + Boolean.toString(tab.getWebContents().isLoadingToDifferentDocument()));
            }
        }
    }

    /**
     * Waits for the given tab to finish loading its current page.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param loadTrigger The trigger action that will result in a page load finished event
     *                    to be fired (not run on the UI thread by default).
     */
    public static void waitForTabPageLoaded(final Tab tab, Runnable loadTrigger)
            throws InterruptedException {
        waitForTabPageLoaded(tab, loadTrigger, CallbackHelper.WAIT_TIMEOUT_SECONDS);
    }

    /**
     * Waits for the given tab to finish loading its current page.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param loadTrigger The trigger action that will result in a page load finished event
     *                    to be fired (not run on the UI thread by default).
     * @param secondsToWait The number of seconds to wait for the page to be loaded.
     */
    public static void waitForTabPageLoaded(
            final Tab tab, Runnable loadTrigger, long secondsToWait)
            throws InterruptedException {
        final CountDownLatch countDownLatch = new CountDownLatch(1);
        final CallbackHelper loadedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabObserver observer =
                        new TabPageLoadedObserver(loadedCallback, null, countDownLatch);
                tab.addObserver(observer);
            }
        });
        loadTrigger.run();
        try {
            loadedCallback.waitForCallback(0, 1, secondsToWait, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Page did not load.  Tab information at time of failure --"
                    + " url: " + tab.getUrl() + ", load progress: " + tab.getProgress()
                    + ", is loading: " + Boolean.toString(tab.isLoading())
                    + ", web contents loading: "
                    + Boolean.toString(tab.getWebContents().isLoadingToDifferentDocument()));
        }
    }

    /**
     * Waits for the given tab to start loading its current page.
     *
     * @param tab The tab to wait for the page loading to be started.
     * @param loadTrigger The trigger action that will result in a page load started event
     *                    to be fired (not run on the UI thread by default).
     * @param secondsToWait The number of seconds to wait for the page to be load to be started.
     */
    public static void waitForTabPageLoadStart(
            final Tab tab, Runnable loadTrigger, long secondsToWait)
            throws InterruptedException {
        final CallbackHelper startedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.addObserver(new EmptyTabObserver() {
                    @Override
                    public void onPageLoadStarted(Tab tab, String url) {
                        startedCallback.notifyCalled();
                        tab.removeObserver(this);
                    }
                });
            }
        });
        loadTrigger.run();
        try {
            startedCallback.waitForCallback(0, 1, secondsToWait, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Page did not start loading.  Tab information at time of failure --"
                    + " url: " + tab.getUrl()
                    + ", load progress: " + tab.getProgress()
                    + ", is loading: " + Boolean.toString(tab.isLoading()));
        }
    }

    /**
     * Switch to the given TabIndex in the current tabModel.
     * @param tabIndex
     */
    public static void switchTabInCurrentTabModel(final ChromeActivity activity,
            final int tabIndex) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(activity.getCurrentTabModel(), tabIndex);
            }
        });
    }

    /**
     * Simulates a click to the normal (not incognito) new tab button.
     * <p>
     * Does not wait for the tab to be loaded.
     */
    public static void clickNewTabButton(Instrumentation instrumentation,
            ChromeTabbedActivity activity) throws InterruptedException {
        final TabModel normalTabModel = activity.getTabModelSelector().getModel(false);
        final CallbackHelper createdCallback = new CallbackHelper();
        normalTabModel.addObserver(
                new EmptyTabModelObserver() {
                    @Override
                    public void didAddTab(Tab tab, TabLaunchType type) {
                        createdCallback.notifyCalled();
                        normalTabModel.removeObserver(this);
                    }
                });
        // Tablet and phone have different new tab buttons; click the right one.
        if (DeviceFormFactor.isTablet()) {
            StripLayoutHelper strip =
                    TabStripUtils.getStripLayoutHelper(activity, false /* incognito */);
            CompositorButton newTabButton = strip.getNewTabButton();
            TabStripUtils.clickCompositorButton(newTabButton, instrumentation, activity);
            instrumentation.waitForIdleSync();
        } else {
            TouchCommon.singleClickView(activity.findViewById(R.id.new_tab_button));
        }

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Never received tab creation event");
        }
    }

    /**
     * Creates a new tab by invoking the 'New Tab' menu item.
     * <p>
     * Returns when the tab has been created and has finished navigating.
     */
    public static void newTabFromMenu(Instrumentation instrumentation,
            final ChromeTabbedActivity activity)
            throws InterruptedException {
        newTabFromMenu(instrumentation, activity, false, true);
    }

    /**
     * Creates a new tab by invoking the 'New Tab' or 'New Incognito Tab' menu item.
     * <p>
     * Returns when the tab has been created and has finished navigating.
     */
    public static void newTabFromMenu(Instrumentation instrumentation,
            final ChromeTabbedActivity activity, boolean incognito, boolean waitForNtpLoad)
            throws InterruptedException {
        final CallbackHelper createdCallback = new CallbackHelper();
        final CallbackHelper selectedCallback = new CallbackHelper();

        TabModel tabModel = activity.getTabModelSelector().getModel(incognito);
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
        tabModel.addObserver(observer);

        MenuUtils.invokeCustomMenuActionSync(instrumentation, activity,
                incognito ? R.id.new_incognito_tab_menu_id : R.id.new_tab_menu_id);

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
        tabModel.removeObserver(observer);

        Tab tab = activity.getActivityTab();
        waitForTabPageLoaded(tab, (String) null);
        if (waitForNtpLoad) NewTabPageTestUtils.waitForNtpLoaded(tab);
        instrumentation.waitForIdleSync();
        Log.d(TAG, "newTabFromMenu <<");
    }

    /**
     * New multiple tabs by invoking the 'new' menu item n times.
     * @param n The number of tabs you want to create.
     */
    public static void newTabsFromMenu(Instrumentation instrumentation,
            ChromeTabbedActivity activity, int n)
            throws InterruptedException {
        while (n > 0) {
            newTabFromMenu(instrumentation, activity);
            --n;
        }
    }

    /**
     * Creates a new tab in the specified model then waits for it to load.
     * <p>
     * Returns when the tab has been created and finishes loading.
     */
    public static void fullyLoadUrlInNewTab(Instrumentation instrumentation,
            final ChromeTabbedActivity activity, final String url, final boolean incognito)
            throws InterruptedException {
        newTabFromMenu(instrumentation, activity, incognito, false);

        final Tab tab = activity.getActivityTab();
        waitForTabPageLoaded(tab, new Runnable(){
            @Override
            public void run() {
                loadUrlOnUiThread(tab, url);
            }
        });
        instrumentation.waitForIdleSync();
    }

    public static void loadUrlOnUiThread(final Tab tab, final String url) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.loadUrl(new LoadUrlParams(url));
            }
        });
    }

    /**
     * Ensure that at least some given number of tabs are open.
     */
    public static void ensureNumOpenTabs(Instrumentation instrumentation,
            ChromeTabbedActivity activity, int newCount)
            throws InterruptedException {
        int curCount = getNumOpenTabs(activity);
        if (curCount < newCount) {
            newTabsFromMenu(instrumentation, activity, newCount - curCount);
        }
    }

    /**
     * Fetch the number of tabs open in the current model.
     */
    public static int getNumOpenTabs(final ChromeActivity activity) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return activity.getCurrentTabModel().getCount();
            }
        });
    }

    /**
     * Closes the current tab through TabModelSelector.
     * <p>
     * Returns after the tab has been closed.
     */
    public static void closeCurrentTab(final Instrumentation instrumentation,
            final ChromeTabbedActivity activity)
            throws InterruptedException {
        closeTabWithAction(instrumentation, activity, new Runnable() {
            @Override
            public void run() {
                instrumentation.runOnMainSync(new Runnable() {
                    @Override
                    public void run() {
                        TabModelUtils.closeCurrentTab(activity.getCurrentTabModel());
                    }
                });
            }
        });
    }

    /**
     * Closes a tab with the given action and waits for a tab closure to be observed.
     */
    public static void closeTabWithAction(Instrumentation instrumentation,
            final ChromeTabbedActivity activity, Runnable action) throws InterruptedException {
        final CallbackHelper closeCallback = new CallbackHelper();
        final TabModelObserver observer = new EmptyTabModelObserver() {
            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                closeCallback.notifyCalled();
            }
        };
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.addObserver(observer);
                }
            }
        });

        action.run();

        try {
            closeCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Tab closed event was never received");
        }
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.removeObserver(observer);
                }
            }
        });
        instrumentation.waitForIdleSync();
        Log.d(TAG, "closeTabWithAction <<");
    }

    /**
     * Close all tabs and waits for all tabs pending closure to be observed.
     */
    public static void closeAllTabs(Instrumentation instrumentation,
            final ChromeTabbedActivity activity) throws InterruptedException {
        final CallbackHelper closeCallback = new CallbackHelper();
        final TabModelObserver observer = new EmptyTabModelObserver() {
            @Override
            public void allTabsPendingClosure(List<Tab> tabs) {
                closeCallback.notifyCalled();
            }
        };
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.addObserver(observer);
                }
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                activity.getTabModelSelector().closeAllTabs();
            }
        });

        try {
            closeCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("All tabs pending closure event was never received");
        }
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.removeObserver(observer);
                }
            }
        });
        instrumentation.waitForIdleSync();
    }

    /**
     * Selects a tab with the given action and waits for the selection event to be observed.
     */
    public static void selectTabWithAction(Instrumentation instrumentation,
            final ChromeTabbedActivity activity, Runnable action) throws InterruptedException {
        final CallbackHelper selectCallback = new CallbackHelper();
        final TabModelObserver observer = new EmptyTabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
                selectCallback.notifyCalled();
            }
        };
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.addObserver(observer);
                }
            }
        });

        action.run();

        try {
            selectCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Tab selected event was never received");
        }
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.removeObserver(observer);
                }
            }
        });
    }
}
