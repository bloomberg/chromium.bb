// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.text.TextUtils;
import android.widget.ListView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionMainMenuItem;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.ChromeHomeIphMenuHeader;
import org.chromium.chrome.browser.widget.bottomsheet.ChromeHomeIphMenuHeader.ChromeHomeIphMenuHeaderTestObserver;
import org.chromium.chrome.browser.widget.bottomsheet.ChromeHomePromoDialog;
import org.chromium.chrome.browser.widget.bottomsheet.ChromeHomePromoDialog.ChromeHomePromoDialogTestObserver;
import org.chromium.chrome.browser.widget.bottomsheet.ChromeHomePromoMenuHeader;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the app menu when Chrome Home is enabled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class ChromeHomeAppMenuTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    private static class TestTracker implements Tracker {
        public CallbackHelper mDimissedCallbackHelper = new CallbackHelper();

        private String mEnabledFeature;

        public TestTracker(String enabledFeature) {
            mEnabledFeature = enabledFeature;
        }

        @Override
        public void notifyEvent(String event) {}

        @Override
        public boolean shouldTriggerHelpUI(String feature) {
            return TextUtils.equals(mEnabledFeature, feature);
        }

        @Override
        public int getTriggerState(String feature) {
            return 0;
        }

        @Override
        public void dismissed(String feature) {
            Assert.assertEquals("Wrong feature dismissed.", mEnabledFeature, feature);
            mDimissedCallbackHelper.notifyCalled();
        }

        @Override
        public boolean isInitialized() {
            return true;
        }

        @Override
        public void addOnInitializedCallback(Callback<Boolean> callback) {}
    }

    private AppMenuHandler mAppMenuHandler;
    private BottomSheet mBottomSheet;
    private EmbeddedTestServer mTestServer;
    private String mTestUrl;

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        mAppMenuHandler = mBottomSheetTestRule.getActivity().getAppMenuHandler();
        mBottomSheet = mBottomSheetTestRule.getBottomSheet();
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);

        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mTestUrl = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    public void testPageMenu() throws IllegalArgumentException, InterruptedException {
        loadTestPage();

        showAppMenuAndAssertMenuShown();
        AppMenu appMenu = mAppMenuHandler.getAppMenu();
        AppMenuIconRowFooter iconRow = (AppMenuIconRowFooter) appMenu.getFooterView();

        assertFalse("Forward button should not be enabled",
                iconRow.getForwardButtonForTests().isEnabled());
        assertTrue("Bookmark button should be enabled",
                iconRow.getBookmarkButtonForTests().isEnabled());
        assertTrue("Download button should be enabled",
                iconRow.getDownloadButtonForTests().isEnabled());
        assertTrue(
                "Info button should be enabled", iconRow.getPageInfoButtonForTests().isEnabled());
        assertTrue(
                "Reload button should be enabled", iconRow.getReloadButtonForTests().isEnabled());

        // Navigate backward, open the menu and assert forward button is enabled.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mAppMenuHandler.hideAppMenu();
            mBottomSheetTestRule.getActivity().getActivityTab().goBack();
        });

        showAppMenuAndAssertMenuShown();
        iconRow = (AppMenuIconRowFooter) appMenu.getFooterView();
        assertTrue(
                "Forward button should be enabled", iconRow.getForwardButtonForTests().isEnabled());
    }

    @Test
    @SmallTest
    public void testTabSwitcherMenu() throws IllegalArgumentException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mBottomSheetTestRule.getActivity().getLayoutManager().showOverview(false));

        showAppMenuAndAssertMenuShown();
        AppMenu appMenu = mAppMenuHandler.getAppMenu();

        assertNull("Footer view should be null", appMenu.getFooterView());
        Assert.assertEquals(
                "There should be four app menu items.", appMenu.getListView().getCount(), 4);
        Assert.assertEquals("'New tab' should be the first item", R.id.new_tab_menu_id,
                appMenu.getListView().getItemIdAtPosition(0));
        Assert.assertEquals("'New incognito tab' should be the second item",
                R.id.new_incognito_tab_menu_id, appMenu.getListView().getItemIdAtPosition(1));
        Assert.assertEquals("'Close all tabs' should be the third item",
                R.id.close_all_tabs_menu_id, appMenu.getListView().getItemIdAtPosition(2));
        Assert.assertEquals("'Settings' should be the fourth item", R.id.preferences_id,
                appMenu.getListView().getItemIdAtPosition(3));
    }

    @Test
    @SmallTest
    public void testNewTabMenu() {
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mBottomSheetTestRule.getActivity(), R.id.new_tab_menu_id);
        ThreadUtils.runOnUiThreadBlocking(() -> mBottomSheet.endAnimations());

        showAppMenuAndAssertMenuShown();
        AppMenu appMenu = mAppMenuHandler.getAppMenu();

        assertNull("Footer view should be null", appMenu.getFooterView());
        Assert.assertEquals(
                "There should be four app menu items.", appMenu.getListView().getCount(), 4);
        Assert.assertEquals("'New incognito tab' should be the first item",
                R.id.new_incognito_tab_menu_id, appMenu.getListView().getItemIdAtPosition(0));
        Assert.assertEquals("'Recent tabs' should be the second item", R.id.recent_tabs_menu_id,
                appMenu.getListView().getItemIdAtPosition(1));
        Assert.assertEquals("'Settings' should be the third item", R.id.preferences_id,
                appMenu.getListView().getItemIdAtPosition(2));
        Assert.assertEquals("'Help & feedback' should be the fourth item", R.id.help_id,
                appMenu.getListView().getItemIdAtPosition(3));
    }

    @Test
    @SmallTest
    public void testNewIncognitoTabMenu() {
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mBottomSheetTestRule.getActivity(), R.id.new_incognito_tab_menu_id);
        ThreadUtils.runOnUiThreadBlocking(() -> mBottomSheet.endAnimations());

        showAppMenuAndAssertMenuShown();
        AppMenu appMenu = mAppMenuHandler.getAppMenu();

        assertNull("Footer view should be null", appMenu.getFooterView());
        Assert.assertEquals(
                "There should be three app menu items.", appMenu.getListView().getCount(), 3);
        Assert.assertEquals("'New tab' should be the first item", R.id.new_tab_menu_id,
                appMenu.getListView().getItemIdAtPosition(0));
        Assert.assertEquals("'Settings' should be the second item", R.id.preferences_id,
                appMenu.getListView().getItemIdAtPosition(1));
        Assert.assertEquals("'Help & feedback' should be the third item", R.id.help_id,
                appMenu.getListView().getItemIdAtPosition(2));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.CHROME_HOME_PROMO})
    public void testPromoAppMenuHeader() throws InterruptedException, TimeoutException {
        // Create a callback to be notified when the dialog is shown.
        final CallbackHelper dialogShownCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeHomePromoDialog.setObserverForTests(new ChromeHomePromoDialogTestObserver() {
                @Override
                public void onDialogShown(ChromeHomePromoDialog shownDialog) {
                    dialogShownCallback.notifyCalled();
                }
            });
        });

        // Load a test page and show the app menu. The header is only shown on the page menu.
        loadTestPage();
        showAppMenuAndAssertMenuShown();

        // Check for the existence of a header.
        ListView listView = mAppMenuHandler.getAppMenu().getListView();
        Assert.assertEquals("There should be one header.", 1, listView.getHeaderViewsCount());

        // Click the header.
        ChromeHomePromoMenuHeader promoHeader = (ChromeHomePromoMenuHeader) listView.findViewById(
                R.id.chrome_home_promo_menu_header);
        ThreadUtils.runOnUiThreadBlocking(() -> { promoHeader.performClick(); });

        // Wait for the dialog to show and the app menu to hide.
        dialogShownCallback.waitForCallback(0);
        assertFalse("Menu should be hidden.", mAppMenuHandler.isAppMenuShowing());

        // Reset state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeHomePromoDialog.setObserverForTests(null); });
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=" + ChromeFeatureList.CHROME_HOME_PROMO})
    public void testIphAppMenuHeader_Click() throws InterruptedException, TimeoutException {
        TestTracker tracker = new TestTracker(FeatureConstants.CHROME_HOME_MENU_HEADER_FEATURE);
        TrackerFactory.setTrackerForTests(tracker);

        // Create a callback to be notified when the menu header is clicked.
        final CallbackHelper menuItemClickedCallback = new CallbackHelper();
        final CallbackHelper menuDismissedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeHomeIphMenuHeader.setObserverForTests(new ChromeHomeIphMenuHeaderTestObserver() {
                @Override
                public void onMenuItemClicked() {
                    menuItemClickedCallback.notifyCalled();
                }

                @Override
                public void onMenuDismissed(boolean dismissIph) {
                    Assert.assertFalse("In-product help should not be dismissed when menu is"
                                    + " dismissed.",
                            dismissIph);
                    menuDismissedCallback.notifyCalled();
                }
            });
        });

        // Load a test page and show the app menu. The header is only shown on the page menu.
        loadTestPage();
        showAppMenuAndAssertMenuShown();

        // Check for the existence of a header.
        ListView listView = mAppMenuHandler.getAppMenu().getListView();
        Assert.assertEquals("There should be one header.", 1, listView.getHeaderViewsCount());

        // Click the header.
        ChromeHomeIphMenuHeader iphHeader =
                (ChromeHomeIphMenuHeader) listView.findViewById(R.id.chrome_home_iph_menu_header);
        ThreadUtils.runOnUiThreadBlocking(() -> { iphHeader.performClick(); });

        // Wait for the app menu to hide, then check state.
        menuItemClickedCallback.waitForCallback(0);
        menuDismissedCallback.waitForCallback(0);
        assertFalse("Menu should be hidden.", mAppMenuHandler.isAppMenuShowing());
        Assert.assertEquals("IPH should not be dimissed yet.", 0,
                tracker.mDimissedCallbackHelper.getCallCount());
        Assert.assertTrue("Bottom sheet help bubble should be showing.",
                mBottomSheet.getHelpBubbleForTests().isShowing());

        // Dismiss the help bubble
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mBottomSheet.getHelpBubbleForTests().dismiss(); });

        Assert.assertEquals(
                "IPH should be dimissed.", 1, tracker.mDimissedCallbackHelper.getCallCount());

        // Reset state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeHomeIphMenuHeader.setObserverForTests(null); });
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=" + ChromeFeatureList.CHROME_HOME_PROMO})
    public void testIphAppMenuHeader_Dismiss() throws InterruptedException, TimeoutException {
        TestTracker tracker = new TestTracker(FeatureConstants.CHROME_HOME_MENU_HEADER_FEATURE);
        TrackerFactory.setTrackerForTests(tracker);

        // Create a callback to be notified when the menu header is clicked.
        final CallbackHelper menuItemClickedCallback = new CallbackHelper();
        final CallbackHelper menuDismissedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeHomeIphMenuHeader.setObserverForTests(new ChromeHomeIphMenuHeaderTestObserver() {
                @Override
                public void onMenuItemClicked() {
                    menuItemClickedCallback.notifyCalled();
                }

                @Override
                public void onMenuDismissed(boolean dismissIph) {
                    Assert.assertTrue("In-product help should be dismissed when menu is"
                                    + " dismissed.",
                            dismissIph);
                    menuDismissedCallback.notifyCalled();
                }
            });
        });

        // Load a test page and show the app menu. The header is only shown on the page menu.
        loadTestPage();
        showAppMenuAndAssertMenuShown();

        // Check for the existence of a header.
        ListView listView = mAppMenuHandler.getAppMenu().getListView();
        Assert.assertEquals("There should be one header.", 1, listView.getHeaderViewsCount());

        // Check that the right header is showing.
        ChromeHomeIphMenuHeader iphHeader =
                (ChromeHomeIphMenuHeader) listView.findViewById(R.id.chrome_home_iph_menu_header);
        Assert.assertNotNull(iphHeader);

        // Hide the app menu.
        ThreadUtils.runOnUiThreadBlocking(() -> { mAppMenuHandler.hideAppMenu(); });

        // Wait for the app menu to hide, then check state.
        menuDismissedCallback.waitForCallback(0);
        Assert.assertEquals("menuItemClickedCallback should not have been called.", 0,
                menuItemClickedCallback.getCallCount());
        Assert.assertEquals(
                "IPH should be dimissed.", 1, tracker.mDimissedCallbackHelper.getCallCount());
        Assert.assertNull(
                "Bottom sheet help bubble should be null.", mBottomSheet.getHelpBubbleForTests());

        // Reset state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeHomeIphMenuHeader.setObserverForTests(null); });
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "disable-features=IPH_ChromeHomeMenuHeader," + ChromeFeatureList.CHROME_HOME_PROMO,
        "enable-features=" + ChromeFeatureList.DATA_REDUCTION_MAIN_MENU})
    public void testDataSaverAppMenuHeader() throws InterruptedException {
        // Load a test page and show the app menu. The header is only shown on the page menu.
        loadTestPage();
        showAppMenuAndAssertMenuShown();

        // There should currently be no headers.
        ListView listView = mAppMenuHandler.getAppMenu().getListView();
        Assert.assertEquals("There should not be a header.", 0, listView.getHeaderViewsCount());

        // Hide the app menu.
        ThreadUtils.runOnUiThreadBlocking(() -> { mAppMenuHandler.hideAppMenu(); });

        // Turn Data Saver on and re-open the menu.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                    mBottomSheetTestRule.getActivity().getApplicationContext(), true);
        });
        showAppMenuAndAssertMenuShown();

        // Check for the existence of a header.
        listView = mAppMenuHandler.getAppMenu().getListView();
        Assert.assertEquals("There should be one header.", 1, listView.getHeaderViewsCount());

        // Check that the right header is showing.
        DataReductionMainMenuItem dataReductionHeader =
                (DataReductionMainMenuItem) listView.findViewById(R.id.data_reduction_menu_item);
        Assert.assertNotNull(dataReductionHeader);
    }

    private void loadTestPage() throws InterruptedException {
        final Tab tab = mBottomSheet.getActiveTab();
        ChromeTabUtils.loadUrlOnUiThread(tab, mTestUrl);
        ChromeTabUtils.waitForTabPageLoaded(tab, mTestUrl);
    }

    private void showAppMenuAndAssertMenuShown() {
        ThreadUtils.runOnUiThread((Runnable) () -> mAppMenuHandler.showAppMenu(null, false));
        CriteriaHelper.pollUiThread(new Criteria("AppMenu did not show") {
            @Override
            public boolean isSatisfied() {
                return mAppMenuHandler.isAppMenuShowing();
            }
        });
    }
}
