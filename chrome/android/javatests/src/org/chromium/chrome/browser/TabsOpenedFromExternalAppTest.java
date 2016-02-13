// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.net.Uri;
import android.os.Environment;
import android.provider.Browser;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.View;

import junit.framework.Assert;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.MultiActivityTestBase;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Test the behavior of tabs when opening a URL from an external app.
 */
public class TabsOpenedFromExternalAppTest extends ChromeTabbedActivityTestBase {

    private static final String EXTERNAL_APP_1_ID = "app1";
    private static final String EXTERNAL_APP_2_ID = "app2";

    static class ElementFocusedCriteria extends Criteria {
        private final Tab mTab;
        private final String mElementId;

        public ElementFocusedCriteria(Tab tab, String elementId) {
            super("Text-field in page not focused.");
            mTab = tab;
            // Add quotes to match returned value from JS.
            mElementId = "\"" + elementId + "\"";
        }

        @Override
        public boolean isSatisfied() {
            String nodeId;
            try {
                StringBuilder sb = new StringBuilder();
                sb.append("(function() {");
                sb.append("  if (document.activeElement && document.activeElement.id) {");
                sb.append("    return document.activeElement.id;");
                sb.append("  }");
                sb.append("  return null;");
                sb.append("})();");

                String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mTab.getWebContents(), sb.toString());
                if (jsonText.equalsIgnoreCase("null") || "".equals(jsonText)) {
                    nodeId = null;
                }
                nodeId = jsonText;
            } catch (InterruptedException e) {
                e.printStackTrace();
                Assert.fail("Failed to retrieve focused node: InterruptedException was thrown");
                return false;
            } catch (TimeoutException e) {
                e.printStackTrace();
                Assert.fail("Failed to retrieve focused node: TimeoutException was thrown");
                return false;
            }
            return TextUtils.equals(mElementId, nodeId);
        }
    }

    static class ElementTextIsCriteria extends Criteria {
        private final Tab mTab;
        private final String mElementId;
        private final String mExpectedText;

        public ElementTextIsCriteria(Tab tab, String elementId, String expectedText) {
            super("Page does not have the text typed in.");
            mTab = tab;
            mElementId = elementId;
            mExpectedText = expectedText;
        }

        @Override
        public boolean isSatisfied() {
            try {
                String text = DOMUtils.getNodeValue(mTab.getWebContents(), mElementId);
                return TextUtils.equals(mExpectedText, text);
            } catch (InterruptedException e) {
                e.printStackTrace();
                return false;
            } catch (TimeoutException e) {
                e.printStackTrace();
                return false;
            }
        }
    }

    private EmbeddedTestServer mTestServer;

    public TabsOpenedFromExternalAppTest() {
        mSkipCheckHttpServer = true;
    }

    @Override
    public void startMainActivity() {
        // We'll start the activity explicitly in the tests, as we need to start it with an intent
        // in a specific test.
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Launch the specified URL as if it was triggered by an external application with id appId.
     * Returns when the URL has been navigated to.
     * @throws InterruptedException
     */
    private void launchUrlFromExternalApp(String url, String appId, boolean createNewTab)
            throws InterruptedException {
        final Intent intent = new Intent(Intent.ACTION_VIEW);
        if (appId != null) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, appId);
        }
        if (createNewTab) {
            intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        }
        intent.setData(Uri.parse(url));

        final Tab originalTab = getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onNewIntent(intent);
            }
        });
        if (createNewTab) {
            CriteriaHelper.pollForUIThreadCriteria(new Criteria("Failed to select different tab") {
                @Override
                public boolean isSatisfied() {
                    return getActivity().getActivityTab() != originalTab;
                }
            });
        }
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), url);
    }

    /**
     * Tests that URLs opened from the same external app don't create new tabs.
     * @throws InterruptedException
     */
    @LargeTest
    @Feature({"Navigation"})
    public void testNoNewTabForSameApp() throws InterruptedException {
        startMainActivityFromLauncher();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        int originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);
        // It should have opened in a new tab.
        int newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url1,
                getActivity().getActivityTab().getUrl());

        // Launch a new URL from the same app, it should open in the same tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url2,
                getActivity().getActivityTab().getUrl());

        // And pressing back should close Clank.
        assertTrue("Window does not have focus before pressing back.",
                getActivity().hasWindowFocus());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onBackPressed();
            }
        });
        getInstrumentation().waitForIdleSync();
        assertFalse("Window still has focus after pressing back.", getActivity().hasWindowFocus());
    }

    /**
     * Tests that URLs opened from an unspecified external app (no Browser.EXTRA_APPLICATION_ID in
     * the intent extras) don't create new tabs.
     * @throws InterruptedException
     */


    @LargeTest
    @Feature({"Navigation"})
    public void testNewTabForUnknownApp() throws InterruptedException {
        startMainActivityFromLauncher();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");


        // Launch a first URL with an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        assertEquals("Selected tab is not on the right URL.", url1,
                getActivity().getActivityTab().getUrl());
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());

        // Launch the same URL without app ID. It should open a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        launchUrlFromExternalApp(url1, null, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url1,
                getActivity().getActivityTab().getUrl());

        // Launch another URL without app ID. It should open a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        launchUrlFromExternalApp(url2, null, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url2,
                getActivity().getActivityTab().getUrl());

        // And pressing back should close Clank.
        assertTrue("Window does not have focus before pressing back.",
                getActivity().hasWindowFocus());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onBackPressed();
            }
        });
        getInstrumentation().waitForIdleSync();
        assertFalse("Window still has focus after pressing back.", getActivity().hasWindowFocus());
    }

    /**
     * Tests that URLs opened with the Browser.EXTRA_CREATE_NEW_TAB extra in
     * the intent do create new tabs.
     * @throws InterruptedException
     */
    @LargeTest
    @Feature({"Navigation"})
    public void testNewTabWithNewTabExtra() throws InterruptedException {
        startMainActivityFromLauncher();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        int originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);
        // It should have opened in a new tab.
        int newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url1,
                getActivity().getActivityTab().getUrl());

        // Launch a new URL from the same app with the right extra to open in a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, true);
        newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url2,
                getActivity().getActivityTab().getUrl());

        // And pressing back should close Clank.
        assertTrue("Window does not have focus before pressing back.",
                getActivity().hasWindowFocus());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onBackPressed();
            }
        });
        getInstrumentation().waitForIdleSync();
        assertFalse("Window still has focus after pressing back.", getActivity().hasWindowFocus());
    }

    /**
     * Similar to testNoNewTabForSameApp but actually starting the application (not just opening a
     * tab) from the external app.
     * @throws InterruptedException
     */
    @LargeTest
    @Feature({"Navigation", "Main"})
    public void testNoNewTabForSameAppOnStart() throws InterruptedException {
        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch Clank from the external app.
        startMainActivityFromExternalApp(url1, EXTERNAL_APP_1_ID);
        assertEquals("Selected tab is not on the right URL.", url1,
                getActivity().getActivityTab().getUrl());

        // Launch a new URL from the same app, it should open in the same tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url2,
                getActivity().getActivityTab().getUrl());

        // And pressing back should close Clank.
        assertTrue("Window does not have focus before pressing back.",
                getActivity().hasWindowFocus());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onBackPressed();
            }
        });
        getInstrumentation().waitForIdleSync();
        assertFalse("Window still has focus after pressing back.", getActivity().hasWindowFocus());
    }

    /**
     * Test that URLs opened from different external apps do create new tabs.
     * @throws InterruptedException
     */
    @LargeTest
    @Feature({"Navigation", "Main"})
    public void testNewTabForDifferentApps() throws InterruptedException {
        startMainActivityFromLauncher();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/test.html");

        // Launch a first URL from an app1.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        int originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());

        // Launch a second URL from an app2, it should open in a new tab.
        launchUrlFromExternalApp(url2, EXTERNAL_APP_2_ID, false);

        // It should have opened in a new tab.
        int newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url2,
                getActivity().getActivityTab().getUrl());

        // Also try with no app id, it should also open in a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        launchUrlFromExternalApp(url3, null, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url3,
                getActivity().getActivityTab().getUrl());
    }

    /**
     * Tests that a tab is not reused when launched from the same app as an already opened tab and
     * when the user has navigated elsewhere manually in the same tab.
     * @throws InterruptedException
     */
    @LargeTest
    @Feature({"Navigation"})
    public void testNewTabAfterNavigation() throws InterruptedException {
        startMainActivityFromLauncher();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/test.html");

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        // Now simulate the user manually navigating to another URL.
        loadUrl(url3);

        // Launch a second URL from the same app, it should open in a new tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url2,
                getActivity().getActivityTab().getUrl());
    }

    /**
     * Tests that a tab is not reused when launched from the same app as an already opened tab and
     * when the user has entered text in the page.
     * @throws InterruptedException
     */
    /**
     * @LargeTest
     * @Feature({"Navigation"})
     * Bug 6467101
     */
    @FlakyTest
    public void testNewTabWhenPageEdited() throws InterruptedException, TimeoutException {
        startMainActivityFromLauncher();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        // Focus the text-field and type something.
        Tab tab = getActivity().getActivityTab();
        DOMUtils.focusNode(tab.getContentViewCore().getWebContents(), "textField");

        // Some processing needs to happen before the test-field has the focus.
        CriteriaHelper.pollForCriteria(new ElementFocusedCriteria(
                getActivity().getActivityTab(), "textField"), 2000, 200);

        // Now type something.
        getInstrumentation().sendStringSync("banana");

        // We also have to wait for the text to happen in the page.
        CriteriaHelper.pollForCriteria(new ElementTextIsCriteria(
                getActivity().getActivityTab(), "textField", "banana"), 2000, 200);

        // Launch a second URL from the same app, it should open in a new tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(getActivity());
        assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        assertEquals("Selected tab is not on the right URL.", url2,
                getActivity().getActivityTab().getUrl());
    }


    private static class TestTabObserver extends EmptyTabObserver {
        private ContextMenu mContextMenu;

        @Override
        public void onContextMenuShown(Tab tab, ContextMenu menu) {
            mContextMenu = menu;
        }
    }

    /**
     * Catches regressions for https://crbug.com/495877.
     * Flakiness reported in https://crbug.com/571030
     */
    @FlakyTest
    @MediumTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    public void testBackgroundSvelteTabIsSelectedAfterClosingExternalTab() throws Exception {
        // Start up Chrome and immediately close its tab -- it gets in the way.
        startMainActivityFromLauncher();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.closeTabByIndex(getActivity().getCurrentTabModel(), 0);
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getTabModelSelector().getTotalTabCount() == 0;
            }
        });

        // Open a tab via an external application.
        final Intent intent = new Intent(
                Intent.ACTION_VIEW, Uri.parse(MultiActivityTestBase.HREF_LINK));
        intent.setClassName(getInstrumentation().getTargetContext().getPackageName(),
                ChromeTabbedActivity.class.getName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, "com.legit.totes");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        getInstrumentation().getTargetContext().startActivity(intent);

        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getTabModelSelector().getTotalTabCount() == 1;
            }
        });
        ApplicationTestUtils.assertWaitForPageScaleFactorMatch(getActivity(), 0.5f, false);

        // Long press the center of the page, which should bring up the context menu.
        final TestTabObserver observer = new TestTabObserver();
        getActivity().getActivityTab().addObserver(observer);
        assertNull(observer.mContextMenu);
        final View view = ThreadUtils.runOnUiThreadBlocking(new Callable<View>() {
            @Override
            public View call() throws Exception {
                return getActivity().getActivityTab().getContentViewCore().getContainerView();
            }
        });
        TouchCommon.longPressView(view);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return observer.mContextMenu != null;
            }
        });
        getActivity().getActivityTab().removeObserver(observer);

        // Select the "open in new tab" option.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(observer.mContextMenu.performIdentifierAction(
                        R.id.contextmenu_open_in_new_tab, 0));
            }
        });

        // The second tab should open in the background.
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getTabModelSelector().getTotalTabCount() == 2;
            }
        });

        // Hitting "back" should close the tab, minimize Chrome, and select the background tab.
        // Confirm that the number of tabs is correct and that closing the tab didn't cause a crash.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().onBackPressed();
            }
        });
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getTabModelSelector().getTotalTabCount() == 1;
            }
        });
    }
}
