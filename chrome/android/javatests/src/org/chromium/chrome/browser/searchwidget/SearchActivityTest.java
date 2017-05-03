// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.searchwidget.SearchActivity.SearchActivityObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.MultiActivityTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.KeyUtils;

/**
 * Tests the {@link SearchActivity}.
 *
 * TODO(dfalcantara): Add tests for:
 *                    + Checking that user can type in the box before native has loaded and still
 *                      have suggestions appear.
 *
 *                    + Performing a search query.
 *                        + Performing a search query while the SearchActivity is alive and the
 *                          default search engine is changed outside the SearchActivity.
 *
 *                    + Handling the promo dialog.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SearchActivityTest {
    private static class TestObserver implements SearchActivityObserver {
        public final CallbackHelper onSetContentViewCallback = new CallbackHelper();
        public final CallbackHelper onFinishNativeInitializationCallback = new CallbackHelper();
        public final CallbackHelper onFinishDeferredInitializationCallback = new CallbackHelper();

        @Override
        public void onSetContentView() {
            onSetContentViewCallback.notifyCalled();
        }

        @Override
        public void onFinishNativeInitialization() {
            onFinishNativeInitializationCallback.notifyCalled();
        }

        @Override
        public void onFinishDeferredInitialization() {
            onFinishDeferredInitializationCallback.notifyCalled();
        }
    }

    @Rule
    @SuppressFBWarnings("URF_UNREAD_PUBLIC_OR_PROTECTED_FIELD")
    public MultiActivityTestRule mTestRule = new MultiActivityTestRule();

    private TestObserver mTestObserver;

    @Before
    public void setUp() {
        mTestObserver = new TestObserver();
        SearchActivity.setObserverForTests(mTestObserver);
    }

    @After
    public void tearDown() {
        SearchActivity.setObserverForTests(null);
    }

    @Test
    @SmallTest
    public void testOmniboxSuggestionContainerAppears() throws Exception {
        Activity searchActivity = fullyStartSearchActivity();

        // Type in anything.  It should force the suggestions to appear.
        setUrlBarText(searchActivity, "anything.");
        final SearchActivityLocationBarLayout locationBar =
                (SearchActivityLocationBarLayout) searchActivity.findViewById(
                        R.id.search_location_bar);
        OmniboxTestUtils.waitForOmniboxSuggestions(locationBar);
    }

    @Test
    @SmallTest
    public void testStartsBrowserAfterUrlSubmitted() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        Activity searchActivity = fullyStartSearchActivity();

        // Monitor for ChromeTabbedActivity.
        ActivityMonitor browserMonitor =
                new ActivityMonitor(ChromeTabbedActivity.class.getName(), null, false);
        instrumentation.addMonitor(browserMonitor);

        // Type in a URL that should get kicked to ChromeTabbedActivity.
        setUrlBarText(searchActivity, "about:blank");
        final UrlBar urlBar = (UrlBar) searchActivity.findViewById(R.id.url_bar);
        KeyUtils.singleKeyEventView(instrumentation, urlBar, KeyEvent.KEYCODE_ENTER);

        // Wait for ChromeTabbedActivity to start.
        final Activity browserActivity = instrumentation.waitForMonitorWithTimeout(
                browserMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", browserActivity);
        Assert.assertTrue(
                "Wrong activity started", browserActivity instanceof ChromeTabbedActivity);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ChromeTabbedActivity chromeActivity = (ChromeTabbedActivity) browserActivity;
                Tab tab = chromeActivity.getActivityTab();
                if (tab == null) return false;

                return TextUtils.equals("about:blank", tab.getUrl());
            }
        });
    }

    private Activity fullyStartSearchActivity() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();

        ActivityMonitor searchMonitor =
                new ActivityMonitor(SearchActivity.class.getName(), null, false);
        instrumentation.addMonitor(searchMonitor);

        // The SearchActivity shouldn't have started yet.
        Assert.assertEquals(0, mTestObserver.onSetContentViewCallback.getCallCount());
        Assert.assertEquals(0, mTestObserver.onFinishNativeInitializationCallback.getCallCount());
        Assert.assertEquals(0, mTestObserver.onFinishDeferredInitializationCallback.getCallCount());

        // Fire the Intent to start up the SearchActivity.
        Intent intent = new Intent();
        SearchWidgetProvider.startSearchActivity(intent, false);
        Activity searchActivity = instrumentation.waitForMonitorWithTimeout(
                searchMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", searchActivity);
        Assert.assertTrue("Wrong activity started", searchActivity instanceof SearchActivity);

        // Wait for the Activity fully load.
        mTestObserver.onSetContentViewCallback.waitForCallback(0);
        mTestObserver.onFinishNativeInitializationCallback.waitForCallback(0);
        mTestObserver.onFinishDeferredInitializationCallback.waitForCallback(0);

        instrumentation.removeMonitor(searchMonitor);
        return searchActivity;
    }

    @SuppressLint("SetTextI18n")
    private void setUrlBarText(final Activity activity, final String url) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                UrlBar urlBar = (UrlBar) activity.findViewById(R.id.url_bar);
                urlBar.setText(url);
            }
        });
    }
}
