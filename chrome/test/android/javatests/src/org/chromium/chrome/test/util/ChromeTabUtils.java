// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Instrumentation;
import android.test.InstrumentationTestCase;
import android.text.TextUtils;

import junit.framework.Assert;

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
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A utility class that contains methods generic to all Tabs tests.
 */
public class ChromeTabUtils {
    private static final String TAG = "ChromeTabUtils";

    /**
     * Waits for the given tab to finish loading it's current page.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param url The URL that will be waited to load for.  Pass in null if loading the
     *            current page is sufficient.
     */
    public static void waitForTabPageLoaded(final Tab tab, final String url)
            throws InterruptedException {
        Assert.assertFalse(ThreadUtils.runningOnUiThread());

        final boolean checkUrl = url != null;

        final CallbackHelper loadedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (!tab.isLoading()
                        && (!checkUrl || TextUtils.equals(tab.getUrl(), url))
                        && !tab.getWebContents().isLoadingToDifferentDocument()) {
                    loadedCallback.notifyCalled();
                    return;
                }

                tab.addObserver(new EmptyTabObserver() {
                    @Override
                    public void onPageLoadFinished(Tab tab) {
                        if (!checkUrl || TextUtils.equals(tab.getUrl(), url)) {
                            loadedCallback.notifyCalled();
                            tab.removeObserver(this);
                        }
                    }
                });
            }
        });

        try {
            loadedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Page did not load.  Tab information at time of failure --"
                    + " url: " + url
                    + ", final URL: " + tab.getUrl()
                    + ", load progress: " + tab.getProgress()
                    + ", is loading: " + Boolean.toString(tab.isLoading()));
        }
    }

    /**
     * Waits for the given tab to finish loading it's current page.
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
     * Waits for the given tab to finish loading it's current page.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param loadTrigger The trigger action that will result in a page load finished event
     *                    to be fired (not run on the UI thread by default).
     * @param secondsToWait The number of seconds to wait for the page to be loaded.
     */
    public static void waitForTabPageLoaded(
            final Tab tab, Runnable loadTrigger, long secondsToWait)
            throws InterruptedException {
        final CallbackHelper loadedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.addObserver(new EmptyTabObserver() {
                    @Override
                    public void onPageLoadFinished(Tab tab) {
                        loadedCallback.notifyCalled();
                        tab.removeObserver(this);
                    }
                });
            }
        });
        loadTrigger.run();
        try {
            loadedCallback.waitForCallback(0, 1, secondsToWait, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Page did not load.  Tab information at time of failure --"
                    + " url: " + tab.getUrl()
                    + ", load progress: " + tab.getProgress()
                    + ", is loading: " + Boolean.toString(tab.isLoading()));
        }
    }

    /**
     * Waits for the given tab to start loading it's current page.
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
    public static void clickNewTabButton(InstrumentationTestCase test,
            ChromeTabbedActivityTestBase base) throws InterruptedException {
        final TabModel normalTabModel = base.getActivity().getTabModelSelector().getModel(false);
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
        if (DeviceFormFactor.isTablet(base.getActivity())) {
            StripLayoutHelper strip =
                    TabStripUtils.getStripLayoutHelper(base.getActivity(), false /* incognito */);
            CompositorButton newTabButton = strip.getNewTabButton();
            TabStripUtils.clickCompositorButton(newTabButton, base);
            test.getInstrumentation().waitForIdleSync();
        } else {
            base.singleClickView(base.getActivity().findViewById(R.id.new_tab_button));
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

    private static void loadUrlOnUiThread(final Tab tab, final String url) {
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
            public void allTabsPendingClosure(List<Integer> tabIds) {
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
