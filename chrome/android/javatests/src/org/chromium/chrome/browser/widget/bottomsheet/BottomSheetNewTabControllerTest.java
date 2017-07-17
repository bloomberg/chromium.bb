// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_PHONE;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.LauncherShortcutActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.widget.FadingBackgroundView;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the NTP UI displayed when Chrome Home is enabled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({BottomSheetTestRule.ENABLE_CHROME_HOME,
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        BottomSheetTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
@Restriction(RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class BottomSheetNewTabControllerTest {
    private FadingBackgroundView mFadingBackgroundView;
    private BottomSheet mBottomSheet;
    private ChromeTabbedActivity mActivity;
    private TabModelSelector mTabModelSelector;
    private TestTabModelObserver mNormalTabModelObserver;
    private TestTabModelObserver mIncognitoTabModelObserver;
    private TestTabModelSelectorObserver mTabModelSelectorObserver;

    /** An observer used to detect changes in the tab model. */
    private static class TestTabModelObserver extends EmptyTabModelObserver {
        private final CallbackHelper mDidCloseTabCallbackHelper = new CallbackHelper();
        private final CallbackHelper mPendingTabAddCallbackHelper = new CallbackHelper();

        @Override
        public void didCloseTab(int tabId, boolean incognito) {
            mDidCloseTabCallbackHelper.notifyCalled();
        }

        @Override
        public void pendingTabAdd(boolean isPendingTabAdd) {
            mPendingTabAddCallbackHelper.notifyCalled();
        }
    }

    private static class TestTabModelSelectorObserver extends EmptyTabModelSelectorObserver {
        private final CallbackHelper mTabModelSelectedCallbackHelper = new CallbackHelper();

        @Override
        public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
            mTabModelSelectedCallbackHelper.notifyCalled();
        }
    }

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    @Before
    public void setUp() throws Exception {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        mBottomSheet = mBottomSheetTestRule.getBottomSheet();
        mActivity = mBottomSheetTestRule.getActivity();
        mTabModelSelector = mActivity.getTabModelSelector();
        mFadingBackgroundView = mActivity.getFadingBackgroundView();
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);

        mNormalTabModelObserver = new TestTabModelObserver();
        mIncognitoTabModelObserver = new TestTabModelObserver();
        mTabModelSelectorObserver = new TestTabModelSelectorObserver();
        mTabModelSelector.getModel(false).addObserver(mNormalTabModelObserver);
        mTabModelSelector.getModel(true).addObserver(mIncognitoTabModelObserver);
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_Normal_FromTab() {
        LayoutManagerChrome layoutManager = mActivity.getLayoutManager();
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        ToolbarDataProvider toolbarDataProvider =
                mActivity.getToolbarManager().getToolbarDataProviderForTests();

        assertEquals("There should be 1 tab.", 1, mTabModelSelector.getTotalTabCount());
        assertFalse("Overview mode should not be showing.", layoutManager.overviewVisible());
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertFalse("Normal model should not be pending tab addition.",
                normalTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", mActivity.getActivityTab(),
                toolbarDataProvider.getTab());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());
        assertTrue("Normal model should not have INVALID_TAB_INDEX",
                normalTabModel.index() != TabModel.INVALID_TAB_INDEX);

        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), mActivity, R.id.new_tab_menu_id);

        // The sheet should be opened at half height over the tab switcher and the tab count should
        // remain unchanged.
        validateState(false, BottomSheet.SHEET_STATE_HALF);
        assertEquals("There should be 1 tab.", 1, mTabModelSelector.getTotalTabCount());
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertTrue(
                "Normal model should be pending tab addition.", normalTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", null, toolbarDataProvider.getTab());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());
        assertTrue("Normal model should have INVALID_TAB_INDEX",
                normalTabModel.index() == TabModel.INVALID_TAB_INDEX);

        // Load a URL in the bottom sheet.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrlInNewTab(new LoadUrlParams("about:blank"));
            }
        });

        // The sheet should now be closed and a regular tab should have been created
        validateState(false, BottomSheet.SHEET_STATE_PEEK);
        assertEquals("There should be 2 tabs.", 2, mTabModelSelector.getTotalTabCount());
        assertFalse("Overview mode not should be showing.", layoutManager.overviewVisible());
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertFalse("Normal model should not be pending tab addition.",
                normalTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", mActivity.getActivityTab(),
                toolbarDataProvider.getTab());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());
        assertTrue("Normal model should not have INVALID_TAB_INDEX",
                normalTabModel.index() != TabModel.INVALID_TAB_INDEX);
    }

    @Test
    @SmallTest
    @FlakyTest(message = "crbug.com/744710")
    public void testNTPOverTabSwitcher_Incognito_FromTab() {
        LayoutManagerChrome layoutManager = mActivity.getLayoutManager();
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        ToolbarDataProvider toolbarDataProvider =
                mActivity.getToolbarManager().getToolbarDataProviderForTests();
        Tab originalTab = mActivity.getActivityTab();

        assertEquals("There should be 1 tab.", 1, mTabModelSelector.getTotalTabCount());
        assertFalse("Overview mode should not be showing.", layoutManager.overviewVisible());
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertFalse("Incognito model should not be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", mActivity.getActivityTab(),
                toolbarDataProvider.getTab());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());

        // Select "New incognito tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.new_incognito_tab_menu_id);
        // The sheet should be opened at full height over the tab switcher and the tab count should
        // remain unchanged. The incognito model should now be selected.
        validateState(false, BottomSheet.SHEET_STATE_FULL);
        assertEquals("There should be 1 tab.", 1, mTabModelSelector.getTotalTabCount());
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertTrue("Incognito model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Incognito model should be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", null, toolbarDataProvider.getTab());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());

        // Check that the previous tab is selected after the sheet is closed without a URL being
        // loaded.
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertFalse("Incognito model should not be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertFalse("Overview mode should not be showing.", layoutManager.overviewVisible());
        assertEquals("Incorrect tab selected.", originalTab, mTabModelSelector.getCurrentTab());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());

        // Select "New incognito tab" from the menu and load a URL.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.new_incognito_tab_menu_id);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrlInNewTab(new LoadUrlParams("about:blank"));
            }
        });

        // The sheet should now be closed and an incognito tab should have been created
        validateState(false, BottomSheet.SHEET_STATE_PEEK);
        assertEquals("There should be 2 tabs.", 2, mTabModelSelector.getTotalTabCount());
        assertTrue(
                "The activity tab should be incognito.", mActivity.getActivityTab().isIncognito());
        assertFalse("Overview mode not should be showing.", layoutManager.overviewVisible());
        assertTrue("Incognito model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertFalse("Incognito model should not be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", mActivity.getActivityTab(),
                toolbarDataProvider.getTab());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());
        assertTrue("Incognito model should not have INVALID_TAB_INDEX",
                incognitoTabModel.index() != TabModel.INVALID_TAB_INDEX);

        // Select "New incognito tab" from the menu again and assert state is as expected. This
        // is designed to exercise IncognitoTabModel#index().
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.new_incognito_tab_menu_id);
        assertTrue("Incognito model should be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", null, toolbarDataProvider.getTab());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());
        assertTrue("Incognito model should have INVALID_TAB_INDEX",
                incognitoTabModel.index() == TabModel.INVALID_TAB_INDEX);
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_Incognito_FromTabSwitcher() {
        final LayoutManagerChrome layoutManager = mActivity.getLayoutManager();
        ToolbarDataProvider toolbarDataProvider =
                mActivity.getToolbarManager().getToolbarDataProviderForTests();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                layoutManager.showOverview(false);
            }
        });

        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.new_incognito_tab_menu_id);

        validateState(false, BottomSheet.SHEET_STATE_FULL);
        assertTrue("Incognito model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_Normal_FromTabSwitcher() {
        final LayoutManagerChrome layoutManager = mActivity.getLayoutManager();
        ToolbarDataProvider toolbarDataProvider =
                mActivity.getToolbarManager().getToolbarDataProviderForTests();

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.new_incognito_tab_menu_id);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.loadUrlInNewTab(new LoadUrlParams("about:blank"));
                layoutManager.showOverview(false);
            }
        });

        assertTrue("Incognito model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), mActivity, R.id.new_tab_menu_id);

        validateState(false, BottomSheet.SHEET_STATE_HALF);
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());

        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        assertTrue("Incognito model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_NoTabs() throws InterruptedException {
        // Close all tabs.
        ChromeTabUtils.closeAllTabs(InstrumentationRegistry.getInstrumentation(), mActivity);
        assertEquals(0, mTabModelSelector.getTotalTabCount());

        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), mActivity, R.id.new_tab_menu_id);

        // The sheet should be opened at full height.
        validateState(false, BottomSheet.SHEET_STATE_FULL);
        assertEquals(0, mTabModelSelector.getTotalTabCount());
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_SwitchModels() {
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        ToolbarDataProvider toolbarDataProvider =
                mActivity.getToolbarManager().getToolbarDataProviderForTests();

        assertFalse("Incognito model should not be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertFalse("Normal model should not be pending tab addition.",
                normalTabModel.isPendingTabAdd());
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());

        // Select "New incognito tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivity, R.id.new_incognito_tab_menu_id);

        assertTrue("Incognito model should be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertFalse("Normal model should not be pending tab addition.",
                normalTabModel.isPendingTabAdd());
        assertTrue("Incognito model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());

        // Select "New tab" from the menu.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), mActivity, R.id.new_tab_menu_id);

        assertFalse("Incognito model should not be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertTrue(
                "Normal model should be pending tab addition.", normalTabModel.isPendingTabAdd());
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_LauncherShortcuts_NormalToIncognito()
            throws InterruptedException, TimeoutException {
        LayoutManagerChrome layoutManager = mActivity.getLayoutManager();
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        ToolbarDataProvider toolbarDataProvider =
                mActivity.getToolbarManager().getToolbarDataProviderForTests();

        assertFalse("Overview mode should not be showing.", layoutManager.overviewVisible());
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertFalse("Normal model should not be pending tab addition.",
                normalTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", mActivity.getActivityTab(),
                toolbarDataProvider.getTab());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());

        // Create a new tab using a launcher shortcut intent.
        int currentPendingTabCalls =
                mIncognitoTabModelObserver.mPendingTabAddCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity.startActivity(LauncherShortcutActivity.getChromeLauncherActivityIntent(
                        mActivity, LauncherShortcutActivity.ACTION_OPEN_NEW_TAB));
            }
        });
        mNormalTabModelObserver.mPendingTabAddCallbackHelper.waitForCallback(
                currentPendingTabCalls);

        // The sheet should be opened at half height over the tab switcher.
        validateState(false, BottomSheet.SHEET_STATE_HALF);
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertTrue(
                "Normal model should be pending tab addition.", normalTabModel.isPendingTabAdd());
        assertEquals("ToolbarDataProvider has incorrect tab.", null, toolbarDataProvider.getTab());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());

        // Create a new incognito tab using an incognito launcher shortcut intent.
        int currentIncognitoPendingTabCalls =
                mIncognitoTabModelObserver.mPendingTabAddCallbackHelper.getCallCount();
        currentPendingTabCalls =
                mNormalTabModelObserver.mPendingTabAddCallbackHelper.getCallCount();
        int currentTabModelSelectedCalls =
                mTabModelSelectorObserver.mTabModelSelectedCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity.startActivity(LauncherShortcutActivity.getChromeLauncherActivityIntent(
                        mActivity, LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB));
            }
        });

        mIncognitoTabModelObserver.mPendingTabAddCallbackHelper.waitForCallback(
                currentIncognitoPendingTabCalls, 1);
        mNormalTabModelObserver.mPendingTabAddCallbackHelper.waitForCallback(
                currentPendingTabCalls, 1);
        mTabModelSelectorObserver.mTabModelSelectedCallbackHelper.waitForCallback(
                currentTabModelSelectedCalls);

        // Check that the state is correct.
        validateState(false, BottomSheet.SHEET_STATE_FULL);
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertTrue("Incognito model should be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertFalse("Normal model should not be pending tab addition.",
                normalTabModel.isPendingTabAdd());
        assertTrue("Incognito model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());
    }

    @Test
    @SmallTest
    public void testNTPOverTabSwitcher_LauncherShortcuts_IncognitoToNormal()
            throws InterruptedException, TimeoutException {
        LayoutManagerChrome layoutManager = mActivity.getLayoutManager();
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        ToolbarDataProvider toolbarDataProvider =
                mActivity.getToolbarManager().getToolbarDataProviderForTests();

        // Create a new tab using an incognito launcher shortcut intent.
        int currentIncognitoPendingTabCalls =
                mIncognitoTabModelObserver.mPendingTabAddCallbackHelper.getCallCount();
        int currentTabModelSelectedCalls =
                mTabModelSelectorObserver.mTabModelSelectedCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity.startActivity(LauncherShortcutActivity.getChromeLauncherActivityIntent(
                        mActivity, LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB));
            }
        });

        mIncognitoTabModelObserver.mPendingTabAddCallbackHelper.waitForCallback(
                currentIncognitoPendingTabCalls, 1);
        mTabModelSelectorObserver.mTabModelSelectedCallbackHelper.waitForCallback(
                currentTabModelSelectedCalls);

        // Check that the state is correct.
        validateState(false, BottomSheet.SHEET_STATE_FULL);
        assertTrue("Overview mode should be showing.", layoutManager.overviewVisible());
        assertTrue("Incognito model should be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertFalse("Normal model should not be pending tab addition.",
                normalTabModel.isPendingTabAdd());
        assertTrue("Incognito model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue("Toolbar should be incognito.", toolbarDataProvider.isIncognito());

        // Create a new tab to ensure that there are no crashes when destroying the incognito
        // profile.
        currentIncognitoPendingTabCalls =
                mIncognitoTabModelObserver.mPendingTabAddCallbackHelper.getCallCount();
        int currentPendingTabCalls =
                mNormalTabModelObserver.mPendingTabAddCallbackHelper.getCallCount();
        currentTabModelSelectedCalls =
                mTabModelSelectorObserver.mTabModelSelectedCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity.startActivity(LauncherShortcutActivity.getChromeLauncherActivityIntent(
                        mActivity, LauncherShortcutActivity.ACTION_OPEN_NEW_TAB));
            }
        });

        mIncognitoTabModelObserver.mPendingTabAddCallbackHelper.waitForCallback(
                currentIncognitoPendingTabCalls, 1);
        mNormalTabModelObserver.mPendingTabAddCallbackHelper.waitForCallback(
                currentPendingTabCalls, 1);
        mTabModelSelectorObserver.mTabModelSelectedCallbackHelper.waitForCallback(
                currentTabModelSelectedCalls);
        validateState(false, BottomSheet.SHEET_STATE_HALF);
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
        assertTrue(
                "Normal model should be pending tab addition.", normalTabModel.isPendingTabAdd());
        assertFalse("Incognito model should not be pending tab addition.",
                incognitoTabModel.isPendingTabAdd());
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());

        // Typically the BottomSheetNewTabController would select the previous model on close. In
        // this case the previous model is incognito because the incognito NTP was showing when
        // the regular new tab was created, but the incognito model has no tabs. Close the bottom
        // sheet and ensure the normal model is still selected.
        currentPendingTabCalls =
                mNormalTabModelObserver.mPendingTabAddCallbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
        assertFalse("Normal model should not be pending tab addition.",
                normalTabModel.isPendingTabAdd());
        mNormalTabModelObserver.mPendingTabAddCallbackHelper.waitForCallback(
                currentPendingTabCalls, 1);
        assertFalse("Toolbar should be normal.", toolbarDataProvider.isIncognito());
        assertFalse("Normal model should be selected.", mTabModelSelector.isIncognitoSelected());
    }

    private void validateState(boolean chromeHomeTabPageSelected, int expectedEndState) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBottomSheet.endAnimations();
            }
        });

        assertEquals("Sheet state incorrect", expectedEndState, mBottomSheet.getSheetState());

        if (chromeHomeTabPageSelected) {
            assertFalse(mFadingBackgroundView.isEnabled());
            assertEquals(0f, mFadingBackgroundView.getAlpha(), 0);
        } else {
            assertTrue(mFadingBackgroundView.isEnabled());
        }
    }
}
