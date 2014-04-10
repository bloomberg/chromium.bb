// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.contextmenu;

import android.test.ActivityInstrumentationTestCase2;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.MenuItem;

import junit.framework.Assert;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.test.util.TabUtils;
import org.chromium.chrome.test.util.TabUtils.TestCallbackHelperContainerForTab;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * A utility class to help open and interact with context menus triggered by a WebContents.
 */
public class ContextMenuUtils {
    /**
     * Opens a context menu.
     * @param testCase              The test harness.
     * @param tab                   The tab to open a context menu for.
     * @param client                The helper client for {@code tab} that can wait for events.  If
     *                              this is {@code null} one will be built automatically.
     * @param openerDOMNodeId       The DOM node to long press to open the context menu for.
     * @return                      The {@link ContextMenu} that was opened.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    public static ContextMenu openContextMenu(ActivityInstrumentationTestCase2 testCase,
            Tab tab, TestCallbackHelperContainerForTab client, String openerDOMNodeId)
                    throws InterruptedException, TimeoutException {
        if (client == null) client = TabUtils.getTestCallbackHelperContainer(tab);

        TestCallbackHelperContainerForTab.OnContextMenuShownHelper helper =
                client.getOnContextMenuShownHelper();

        int callCount = helper.getCallCount();
        DOMUtils.longPressNode(testCase, tab.getContentView(), openerDOMNodeId);

        helper.waitForCallback(callCount);
        return helper.getContextMenu();
    }

    /**
     * Opens and selects an item from a context menu.
     * @param testCase              The test harness.
     * @param tab                   The tab to open a context menu for.
     * @param client                The helper client for {@code tab} that can wait for events.  If
     *                              this is {@code null} one will be built automatically.
     * @param openerDOMNodeId       The DOM node to long press to open the context menu for.
     * @param itemId                The context menu item ID to select.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    public static void selectContextMenuItem(ActivityInstrumentationTestCase2 testCase,
            Tab tab, TestCallbackHelperContainerForTab client, String openerDOMNodeId,
            final int itemId) throws InterruptedException, TimeoutException {
        ContextMenu menu = openContextMenu(testCase, tab, client, openerDOMNodeId);
        Assert.assertNotNull("Failed to open context menu", menu);

        selectOpenContextMenuItem(testCase, menu, itemId);
    }

    /**
     * Opens and selects an item from a context menu.
     * @param testCase              The test harness.
     * @param tab                   The tab to open a context menu for.
     * @param client                The helper client for {@code tab} that can wait for events.  If
     *                              this is {@code null} one will be built automatically.
     * @param openerDOMNodeId       The DOM node to long press to open the context menu for.
     * @param itemTitle             The title of the context menu item to select.
     * @throws InterruptedException
     * @throws TimeoutException
     */
    public static void selectContextMenuItemByTitle(ActivityInstrumentationTestCase2 testCase,
            Tab tab, TestCallbackHelperContainerForTab client, String openerDOMNodeId,
            String itemTitle) throws InterruptedException, TimeoutException {

        ContextMenu menu = openContextMenu(testCase, tab, client, openerDOMNodeId);
        Assert.assertNotNull("Failed to open context menu", menu);

        Integer itemId = null;
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (TextUtils.equals(item.getTitle(), itemTitle)) {
                itemId = item.getItemId();
                break;
            }
        }
        Assert.assertNotNull("Couldn't find context menu item for '" + itemTitle + "'", itemId);

        selectOpenContextMenuItem(testCase, menu, itemId);
    }

    private static void selectOpenContextMenuItem(final ActivityInstrumentationTestCase2 testCase,
            final ContextMenu menu, final int itemId) throws InterruptedException {
        MenuItem item = menu.findItem(itemId);
        Assert.assertNotNull("Could not find '" + itemId + "' in menu", item);
        Assert.assertTrue("'" + itemId + "' is not visible", item.isVisible());
        Assert.assertTrue("'" + itemId + "' is not enabled", item.isEnabled());

        testCase.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                boolean activated = menu.performIdentifierAction(itemId, 0);
                Assert.assertTrue("Failed to activate '" + itemId + "' in menu", activated);
            }
        });

        Assert.assertTrue("Activity did not regain focus.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return testCase.getActivity().hasWindowFocus();
                    }
                }));
    }
}