// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.pm.ActivityInfo;
import android.test.suitebuilder.annotation.LargeTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.concurrent.TimeoutException;

/**
 * Test suite for the TabStrip and making sure it properly represents the TabModel backend.
 */
public class TabStripTest extends ChromeTabbedActivityTestBase {

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Tests that the initial state of the system is good.  This is so the default TabStrips match
     * the TabModels and we do not have to test this in further tests.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testInitialState() throws InterruptedException {
        getInstrumentation().waitForIdleSync();
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that pressing the new tab button creates a new tab, properly updating the selected
     * index.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip", "Main"})
    public void testNewTabButtonWithOneTab() throws InterruptedException {
        getInstrumentation().waitForIdleSync();
        assertEquals("Expected original tab to be selected",
                getActivity().getTabModelSelector().getModel(false).index(), 0);

        ChromeTabUtils.clickNewTabButton(this, this);

        getInstrumentation().waitForIdleSync();
        assertEquals("Expected two tabs to exist",
                getActivity().getTabModelSelector().getModel(false).getCount(), 2);
        compareAllTabStripsWithModel();
        assertEquals("Expected second tab to be selected",
                getActivity().getTabModelSelector().getModel(false).index(), 1);
    }

    /**
     * Tests that pressing the new tab button creates a new tab when many exist, properly updating
     * the selected index.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testNewTabButtonWithManyTabs() throws InterruptedException {
        ChromeTabUtils.newTabsFromMenu(getInstrumentation(), getActivity(), 3);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(getActivity().getTabModelSelector().getModel(false), 0);
            }
        });
        getInstrumentation().waitForIdleSync();
        assertEquals("Expected original tab to be selected",
                getActivity().getTabModelSelector().getModel(false).index(), 0);
        compareAllTabStripsWithModel();

        ChromeTabUtils.clickNewTabButton(this, this);

        getInstrumentation().waitForIdleSync();
        assertEquals("Expected five tabs to exist",
                getActivity().getTabModelSelector().getModel(false).getCount(), 5);
        assertEquals("Expected last tab to be selected",
                getActivity().getTabModelSelector().getModel(false).index(), 4);
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that creating a new tab from the menu properly updates the TabStrip.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testNewTabFromMenu() throws InterruptedException {
        getInstrumentation().waitForIdleSync();
        compareAllTabStripsWithModel();
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        getInstrumentation().waitForIdleSync();
        assertEquals("Expected two tabs to exist",
                getActivity().getTabModelSelector().getModel(false).getCount(), 2);
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that creating a new incognito from the menu properly updates the TabStrips and
     * activates the incognito TabStrip.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testNewIncognitoTabFromMenuAtNormalStrip() throws InterruptedException {
        getInstrumentation().waitForIdleSync();
        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        newIncognitoTabFromMenu();
        getInstrumentation().waitForIdleSync();
        compareAllTabStripsWithModel();
        assertTrue("Expected incognito strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        assertEquals("Expected normal model to have one tab",
                getActivity().getTabModelSelector().getModel(false).getCount(), 1);
        assertEquals("Expected incognito model to have one tab",
                getActivity().getTabModelSelector().getModel(false).getCount(), 1);
    }

    /**
     * Tests that selecting a tab properly selects the new tab.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testSelectWithTwoTabs() throws InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        getInstrumentation().waitForIdleSync();
        assertEquals("The second tab is not selected",
                getActivity().getCurrentTabModel().index(), 1);
        selectTab(false, getActivity().getCurrentTabModel().getTabAt(0).getId());
        getInstrumentation().waitForIdleSync();
        assertEquals("The first tab is not selected",
                getActivity().getCurrentTabModel().index(), 0);
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that selecting a tab properly selects the new tab with many present.  This lets us
     * also check that the visible tab ordering is correct.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testSelectWithManyTabs() throws InterruptedException {
        ChromeTabUtils.newTabsFromMenu(getInstrumentation(), getActivity(), 4);
        getInstrumentation().waitForIdleSync();
        assertEquals("The last tab is not selected",
                getActivity().getCurrentTabModel().index(), 4);
        compareAllTabStripsWithModel();
        // Note: if the tab is not visible, this will fail. Currently that's not a problem, because
        // the devices we test on are wide enough.
        selectTab(false, getActivity().getCurrentTabModel().getTabAt(0).getId());
        getInstrumentation().waitForIdleSync();
        assertEquals("The middle tab is not selected",
                getActivity().getCurrentTabModel().index(), 0);
        compareAllTabStripsWithModel();
    }

    /**
     * Tests closing a tab when there are two tabs open.  The remaining tab should still be
     * selected.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testCloseTabWithTwoTabs() throws InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        getInstrumentation().waitForIdleSync();
        assertEquals("There are not two tabs present",
                getActivity().getCurrentTabModel().getCount(), 2);
        assertEquals("The second tab is not selected",
                getActivity().getCurrentTabModel().index(), 1);
        int initialSelectedId = getActivity().getActivityTab().getId();
        closeTab(false, getActivity().getCurrentTabModel().getTabAt(0).getId());
        getInstrumentation().waitForIdleSync();
        assertEquals("There is not one tab present",
                getActivity().getCurrentTabModel().getCount(), 1);
        assertEquals("The wrong tab index is selected after close",
                getActivity().getCurrentTabModel().index(), 0);
        assertEquals("Same tab not still selected", initialSelectedId,
                getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests closing a tab when there are many tabs open.  The remaining tab should still be
     * selected, even if the index has changed.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testCloseTabWithManyTabs() throws InterruptedException {
        ChromeTabUtils.newTabsFromMenu(getInstrumentation(), getActivity(), 4);
        getInstrumentation().waitForIdleSync();
        assertEquals("There are not five tabs present",
                getActivity().getCurrentTabModel().getCount(), 5);
        assertEquals("The last tab is not selected",
                getActivity().getCurrentTabModel().index(), 4);
        int initialSelectedId = getActivity().getActivityTab().getId();
        // Note: if the tab is not visible, this will fail. Currently that's not a problem, because
        // the devices we test on are wide enough.
        closeTab(false, getActivity().getCurrentTabModel().getTabAt(0).getId());
        getInstrumentation().waitForIdleSync();
        assertEquals("There are not four tabs present",
                getActivity().getCurrentTabModel().getCount(), 4);
        assertEquals("The wrong tab index is selected after close",
                getActivity().getCurrentTabModel().index(), 3);
        assertEquals("Same tab not still selected", initialSelectedId,
                getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that closing the selected tab properly closes the current tab and updates to a new
     * selected tab.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testCloseSelectedTab() throws InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        getInstrumentation().waitForIdleSync();
        assertEquals("There are not two tabs present",
                getActivity().getCurrentTabModel().getCount(), 2);
        assertEquals("The second tab is not selected",
                getActivity().getCurrentTabModel().index(), 1);
        int newSelectionId = getActivity().getCurrentTabModel().getTabAt(0).getId();
        closeTab(false, getActivity().getCurrentTabModel().getTabAt(1).getId());
        getInstrumentation().waitForIdleSync();
        assertEquals("There is not one tab present",
                getActivity().getCurrentTabModel().getCount(), 1);
        assertEquals("The wrong tab index is selected after close",
                getActivity().getCurrentTabModel().index(), 0);
        assertEquals("New tab not selected", newSelectionId,
                getActivity().getActivityTab().getId());
        compareAllTabStripsWithModel();
    }

    /**
     * Tests that selecting "Close all tabs" from the tab menu closes all tabs.
     * Also tests that long press on close button selects the tab and displays the menu.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testCloseAllTabsFromTabMenuClosesAllTabs() throws InterruptedException {
        // 1. Create a second tab
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());
        getInstrumentation().waitForIdleSync();
        assertEquals("There are not two tabs present",
                2, getActivity().getCurrentTabModel().getCount());
        assertEquals("The second tab is not selected",
                1, getActivity().getCurrentTabModel().index());

        // 2. Display tab menu on first tab
        int tabSelectionId = getActivity().getCurrentTabModel().getTabAt(0).getId();
        longPressCloseTab(false, tabSelectionId);
        getInstrumentation().waitForIdleSync();
        assertEquals("There are not two tabs present",
                2, getActivity().getCurrentTabModel().getCount());
        assertEquals("The wrong tab index is selected after long press",
                0, getActivity().getCurrentTabModel().index());
        assertEquals("Long pressed tab not selected",
                tabSelectionId, getActivity().getActivityTab().getId());

        // 3. Invoke "close all tabs" menu action; block until action is completed
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabStripUtils.getActiveStripLayoutHelper(getActivity()).clickTabMenuItem(
                        StripLayoutHelper.ID_CLOSE_ALL_TABS);
            }
        });

        // 4. Ensure all tabs were closed
        assertEquals("Expected no tabs to be present",
                0, getActivity().getCurrentTabModel().getCount());
    }

    /**
     * Tests that the tab menu is dismissed when the orientation changes and no tabs
     * are closed.
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testTabMenuDismissedOnOrientationChange() throws InterruptedException {
        // 1. Set orientation to portrait
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        getInstrumentation().waitForIdleSync();

        // 2. Open tab menu
        int tabSelectionId = getActivity().getCurrentTabModel().getTabAt(0).getId();
        longPressCloseTab(false, tabSelectionId);
        getInstrumentation().waitForIdleSync();

        // 3. Set orientation to landscape and assert tab menu is not showing
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        getInstrumentation().waitForIdleSync();
        assertFalse(TabStripUtils.getActiveStripLayoutHelper(getActivity()).isTabMenuShowing());
        assertEquals("Expected 1 tab to be present",
                1, getActivity().getCurrentTabModel().getCount());

        // 4. Reset orientation
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        getInstrumentation().waitForIdleSync();
    }

    /**
     * Tests that pressing the incognito toggle button properly switches between the incognito
     * and normal TabStrips.
     * @throws InterruptedException
     */

    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testToggleIncognitoMode() throws InterruptedException {
        getInstrumentation().waitForIdleSync();
        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        newIncognitoTabFromMenu();
        getInstrumentation().waitForIdleSync();
        assertTrue("Expected incognito strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        compareAllTabStripsWithModel();
        clickIncognitoToggleButton();
        getInstrumentation().waitForIdleSync();
        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        compareAllTabStripsWithModel();
        clickIncognitoToggleButton();
        getInstrumentation().waitForIdleSync();
        assertTrue("Expected incognito strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Tests that closing the last incognito tab properly closes the incognito TabStrip and
     * switches to the normal TabStrip.
     * @throws InterruptedException
     */
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    public void testCloseLastIncognitoTab() throws InterruptedException {
        getInstrumentation().waitForIdleSync();
        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        newIncognitoTabFromMenu();
        getInstrumentation().waitForIdleSync();
        assertTrue("Expected incognito strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        closeTab(true, TabModelUtils.getCurrentTab(
                getActivity().getTabModelSelector().getModel(true)).getId());
        getInstrumentation().waitForIdleSync();
        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        assertEquals("Expected incognito strip to have no tabs",
                getActivity().getTabModelSelector().getModel(true).getCount(), 0);
    }

    /**
     * Tests that closing all incognito tab properly closes the incognito TabStrip and
     * switches to the normal TabStrip.
     * @throws InterruptedException
     */
    @LargeTest
    @Feature({"TabStrip"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    public void testCloseAllIncognitoTabsFromTabMenu() throws InterruptedException {
        //1. Create two incognito tabs
        getInstrumentation().waitForIdleSync();
        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        newIncognitoTabFromMenu();
        getInstrumentation().waitForIdleSync();
        newIncognitoTabFromMenu();
        getInstrumentation().waitForIdleSync();
        assertTrue("Expected incognito strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        assertEquals("Expected incognito strip to have 2 tabs",
                2, getActivity().getTabModelSelector().getModel(true).getCount());

        // 2. Open tab menu
        int tabSelectionId =  TabModelUtils.getCurrentTab(
                getActivity().getTabModelSelector().getModel(true)).getId();
        longPressCloseTab(true, tabSelectionId);
        getInstrumentation().waitForIdleSync();

        // 3. Invoke menu action; block until action is completed
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabStripUtils.getActiveStripLayoutHelper(getActivity()).clickTabMenuItem(
                        StripLayoutHelper.ID_CLOSE_ALL_TABS);
            }
        });

        // 4. Ensure all incognito tabs were closed and TabStrip is switched to normal
        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        assertEquals("Expected normal strip to have 1 tab",
                1, getActivity().getTabModelSelector().getModel(false).getCount());
        assertEquals("Expected incognito strip to have no tabs",
                0, getActivity().getTabModelSelector().getModel(true).getCount());
    }

    /**
     * Test that switching a tab and quickly changing the model stays on the correct new tab/model
     * when the tab finishes loading (when the GL overlay goes away).
     * @throws InterruptedException
     */
    @LargeTest
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_TABLET)
    @Feature({"TabStrip"})
    public void testTabSelectionViewDoesNotBreakModelSwitch() throws InterruptedException {
        getInstrumentation().waitForIdleSync();
        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        newIncognitoTabFromMenu();
        getInstrumentation().waitForIdleSync();
        assertTrue("Expected incognito strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
        newIncognitoTabFromMenu();
        ChromeTabUtils.switchTabInCurrentTabModel(getActivity(), 0);

        clickIncognitoToggleButton();

        assertFalse("Expected normal strip to be selected",
                getActivity().getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Take a model index and figure out which index it will be in the TabStrip's view hierarchy.
     * @param tabCount The number of tabs.
     * @param selectedIndex The index of the selected tab.
     * @param modelPos The position in the model we want to map.
     * @return The position in the view hierarchy that represents the tab at modelPos.
     */
    private int mapModelToViewIndex(int tabCount, int selectedIndex, int modelPos) {
        if (modelPos < selectedIndex) {
            return modelPos;
        } else if (modelPos == selectedIndex) {
            return tabCount - 1;
        } else {
            return tabCount - 1 - modelPos + selectedIndex;
        }
    }

    /**
     * Simulates a click to the incognito toggle button.
     */
    protected void clickIncognitoToggleButton() throws InterruptedException {
        final CallbackHelper tabModelSelectedCallback = new CallbackHelper();
        TabModelSelectorObserver observer = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                tabModelSelectedCallback.notifyCalled();
            }
        };
        getActivity().getTabModelSelector().addObserver(observer);
        StripLayoutHelperManager manager = TabStripUtils.getStripLayoutHelperManager(getActivity());
        TabStripUtils.clickCompositorButton(manager.getModelSelectorButton(), this);
        try {
            tabModelSelectedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            fail("Tab model selected event never occurred.");
        }
        getActivity().getTabModelSelector().removeObserver(observer);
    }

    /**
     * Simulates a click on a tab, selecting it.
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void selectTab(final boolean incognito, final int id) throws InterruptedException {
        ChromeTabUtils.selectTabWithAction(getInstrumentation(), getActivity(), new Runnable() {
            @Override
            public void run() {
                TabStripUtils.clickTab(
                        TabStripUtils.findStripLayoutTab(getActivity(), incognito, id),
                        TabStripTest.this);
            }
        });
    }

    /**
     * Simulates a click on the close button of a tab.
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void closeTab(final boolean incognito, final int id) throws InterruptedException {
        ChromeTabUtils.closeTabWithAction(getInstrumentation(), getActivity(), new Runnable() {
            @Override
            public void run() {
                StripLayoutTab tab = TabStripUtils.findStripLayoutTab(
                        getActivity(), incognito, id);
                TabStripUtils.clickCompositorButton(tab.getCloseButton(), TabStripTest.this);
            }
        });
    }

    /**
     * Simulates a long press on the close button of a tab. Asserts that the tab
     * is selected and the tab menu is showing.
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void longPressCloseTab(final boolean incognito, final int id)
            throws InterruptedException {
        ChromeTabUtils.selectTabWithAction(getInstrumentation(), getActivity(), new Runnable() {
            @Override
            public void run() {
                StripLayoutTab tab = TabStripUtils.findStripLayoutTab(
                        getActivity(), incognito, id);
                TabStripUtils.longPressCompositorButton(tab.getCloseButton(), TabStripTest.this);
            }
        });
        assertTrue(TabStripUtils.getActiveStripLayoutHelper(getActivity()).isTabMenuShowing());
    }

    /**
     * Compares a TabView with the corresponding model Tab.  This tries to compare as many
     * features as possible making sure the TabView properly mirrors the Tab it represents.
     * @param incognito Whether or not this tab is incognito or not.
     * @param id The id of the tab to compare.
     */
    protected void compareTabViewWithModel(boolean incognito, int id) {
        TabModel model = getActivity().getTabModelSelector().getModel(incognito);
        Tab tab = TabModelUtils.getTabById(model, id);
        StripLayoutHelper tabStrip = TabStripUtils.getStripLayoutHelper(getActivity(), incognito);
        StripLayoutTab tabView = TabStripUtils.findStripLayoutTab(getActivity(), incognito, id);

        assertTrue("One of Tab and TabView does not exist", (tabView == null && tab == null)
                || (tabView != null && tab != null));

        if (tabView == null || tab == null) return;

        assertEquals("The IDs are not identical", tabView.getId(), tab.getId());

        int assumedTabViewIndex = mapModelToViewIndex(model.getCount(), model.index(),
                model.indexOf(tab));

        assertEquals("The tab is not in the proper position ",
                assumedTabViewIndex, tabStrip.visualIndexOfTab(tabView));

        if (TabModelUtils.getCurrentTab(model) == tab
                && getActivity().getTabModelSelector().isIncognitoSelected() == incognito) {
            assertTrue("ChromeTab is not in the proper selection state",
                    tabStrip.isForegroundTab(tabView));
            assertEquals("ChromeTab is not completely visible, but is selected",
                    tabView.getVisiblePercentage(), 1.0f);
        }

        // TODO(dtrainor): Compare favicon bitmaps?  Only compare a few pixels.
    }

    /**
     * Compares an entire TabStrip with the corresponding TabModel.  This tries to compare
     * as many features as possible, including checking all of the tabs through
     * compareTabViewWithModel.  It also checks that the incognito indicator is visible if the
     * incognito tab is showing.
     * @param incognito Whether or not to check the incognito or normal TabStrip.
     */
    protected void compareTabStripWithModel(boolean incognito) {
        TabModel model = getActivity().getTabModelSelector().getModel(incognito);
        StripLayoutHelper strip = TabStripUtils.getStripLayoutHelper(getActivity(), incognito);
        StripLayoutHelper activeStrip = TabStripUtils.getActiveStripLayoutHelper(getActivity());
        TabModel activeModel = getActivity().getCurrentTabModel();

        if (activeModel.isIncognito() == incognito) {
            assertEquals("TabStrip is not in the right visible state", strip, activeStrip);
            assertEquals("TabStrip does not have the same number of views as the model",
                    strip.getTabCount(), model.getCount());
        } else {
            assertTrue("TabStrip is not in the right visible state", model != activeModel);
        }

        CompositorButton incognitoIndicator =
                TabStripUtils.getStripLayoutHelperManager(getActivity()).getModelSelectorButton();
        if (activeModel.isIncognito()) {
            assertNotNull("Incognito indicator null in incognito mode", incognitoIndicator);
            assertTrue("Incognito indicator not visible in incognito mode",
                    incognitoIndicator.isVisible());
        } else if (getActivity().getTabModelSelector().getModel(true).getCount() == 0) {
            assertFalse("Incognito indicator visible in non incognito mode",
                    incognitoIndicator.isVisible());
        }

        for (int i = 0; i < model.getCount(); ++i) {
            compareTabViewWithModel(incognito, model.getTabAt(i).getId());
        }
    }

    /**
     * Compares all TabStrips with the corresponding TabModels.  This also checks if the incognito
     * toggle is visible if necessary.
     */
    protected void compareAllTabStripsWithModel() {
        compareTabStripWithModel(true);
        compareTabStripWithModel(false);
    }
}
