// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Dialog;
import android.support.test.filters.LargeTest;
import android.test.UiThreadTest;
import android.text.SpannableString;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.util.concurrent.Callable;

/**
 * Tests for the ItemChooserDialog class.
 */
@RetryOnFailure
public class ItemChooserDialogTest extends ChromeActivityTestCaseBase<ChromeActivity>
        implements ItemChooserDialog.ItemSelectedCallback {

    ItemChooserDialog mChooserDialog;

    String mLastSelectedId = "None";

    public ItemChooserDialogTest() {
        super(ChromeActivity.class);
    }

    // ChromeActivityTestCaseBase:

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mChooserDialog = createDialog();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    // ItemChooserDialog.ItemSelectedCallback:

    @Override
    public void onItemSelected(String id) {
        mLastSelectedId = id;
    }

    private ItemChooserDialog createDialog() {
        SpannableString title = new SpannableString("title");
        SpannableString searching = new SpannableString("searching");
        SpannableString noneFound = new SpannableString("noneFound");
        SpannableString statusActive = new SpannableString("statusActive");
        SpannableString statusIdleNoneFound = new SpannableString("statusIdleNoneFound");
        SpannableString statusIdleSomeFound = new SpannableString("statusIdleSomeFound");
        String positiveButton = new String("positiveButton");
        final ItemChooserDialog.ItemChooserLabels labels =
                new ItemChooserDialog.ItemChooserLabels(title, searching, noneFound, statusActive,
                        statusIdleNoneFound, statusIdleSomeFound, positiveButton);
        ItemChooserDialog dialog = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<ItemChooserDialog>() {
                        @Override
                        public ItemChooserDialog call() {
                            ItemChooserDialog dialog = new ItemChooserDialog(
                                    getActivity(), ItemChooserDialogTest.this, labels);
                            return dialog;
                        }
                });
        return dialog;
    }

    private void selectItem(Dialog dialog, int position, String expectedItemId,
            boolean expectedEnabledState) {
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return items.getChildAt(0) != null;
            }
        });

        // Verify first item selected gets selected.
        TouchCommon.singleClickView(items.getChildAt(position - 1));

        CriteriaHelper.pollUiThread(
                Criteria.equals(expectedEnabledState, new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return button.isEnabled();
                    }
                }));

        if (!expectedEnabledState) return;

        TouchCommon.singleClickView(button);

        CriteriaHelper.pollUiThread(
                Criteria.equals(expectedItemId, new Callable<String>() {
                    @Override
                    public String call() {
                        return mLastSelectedId;
                    }
                }));
    }

    @LargeTest
    public void testSimpleItemSelection() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = (TextViewWithClickableSpans)
                dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        // Before we add items to the dialog, the 'searching' message should be
        // showing, the Commit button should be disabled and the list view hidden.
        assertEquals("searching", statusView.getText().toString());
        assertFalse(button.isEnabled());
        assertEquals(View.GONE, items.getVisibility());

        mChooserDialog.addOrUpdateItem("key1", "desc1");
        mChooserDialog.addOrUpdateItem("key2", "desc2");

        // Two items showing, the empty view should be no more and the button
        // should now be enabled.
        assertEquals(View.VISIBLE, items.getVisibility());
        assertEquals(View.GONE, items.getEmptyView().getVisibility());
        assertEquals("statusActive", statusView.getText().toString());
        assertFalse(button.isEnabled());

        mChooserDialog.setIdleState();
        // After discovery stops the list should be visible with two items,
        // it should not show the empty view and the button should not be enabled.
        // The chooser should show the status idle text.
        assertEquals(View.VISIBLE, items.getVisibility());
        assertEquals(View.GONE, items.getEmptyView().getVisibility());
        assertEquals("statusIdleSomeFound", statusView.getText().toString());
        assertFalse(button.isEnabled());

        // Select the first item and verify it got selected.
        selectItem(dialog, 1, "key1", true);

        mChooserDialog.dismiss();
    }

    @LargeTest
    public void testNoItemsAddedDiscoveryIdle() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = (TextViewWithClickableSpans)
                dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        // Before we add items to the dialog, the 'searching' message should be
        // showing, the Commit button should be disabled and the list view hidden.
        assertEquals("searching", statusView.getText().toString());
        assertFalse(button.isEnabled());
        assertEquals(View.GONE, items.getVisibility());

        mChooserDialog.setIdleState();

        // Listview should now be showing empty, with an empty view visible to
        // drive home the point and a status message at the bottom.
        assertEquals(View.GONE, items.getVisibility());
        assertEquals(View.VISIBLE, items.getEmptyView().getVisibility());
        assertEquals("statusIdleNoneFound", statusView.getText().toString());
        assertFalse(button.isEnabled());

        mChooserDialog.dismiss();
    }

    @LargeTest
    public void testDisabledSelection() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        mChooserDialog.addOrUpdateItem("key1", "desc1");
        mChooserDialog.addOrUpdateItem("key2", "desc2");

        // Disable one item and try to select it.
        mChooserDialog.setEnabled("key1", false);
        selectItem(dialog, 1, "None", false);
        // The other is still selectable.
        selectItem(dialog, 2, "key2", true);

        mChooserDialog.dismiss();
    }

    @LargeTest
    public void testPairButtonDisabledOrEnabledAfterSelectedItemDisabledOrEnabled() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        final Button button = (Button) dialog.findViewById(R.id.positive);

        mChooserDialog.addOrUpdateItem("key1", "desc1");
        mChooserDialog.addOrUpdateItem("key2", "desc2");

        selectItem(dialog, 1, "key1", true);
        assertTrue(button.isEnabled());

        mChooserDialog.setEnabled("key1", false);
        assertFalse(button.isEnabled());

        mChooserDialog.setEnabled("key1", true);
        assertTrue(button.isEnabled());

        mChooserDialog.dismiss();
    }

    @LargeTest
    public void testPairButtonDisabledAfterSelectedItemRemoved() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        final Button button = (Button) dialog.findViewById(R.id.positive);

        mChooserDialog.addOrUpdateItem("key1", "desc1");
        mChooserDialog.addOrUpdateItem("key2", "desc2");

        selectItem(dialog, 1, "key1", true);
        assertTrue(button.isEnabled());

        mChooserDialog.removeItemFromList("key1");
        assertFalse(button.isEnabled());

        mChooserDialog.dismiss();
    }

    @LargeTest
    public void testSelectAnItemAndRemoveAnotherItem() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        final Button button = (Button) dialog.findViewById(R.id.positive);
        ItemChooserDialog.ItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

        mChooserDialog.addOrUpdateItem("key1", "desc1");
        mChooserDialog.addOrUpdateItem("key2", "desc2");
        mChooserDialog.addOrUpdateItem("key3", "desc3");

        selectItem(dialog, 2, "key2", true);
        assertTrue(button.isEnabled());

        // Remove the item before the currently selected item.
        mChooserDialog.removeItemFromList("key1");
        assertTrue(button.isEnabled());
        assertEquals("key2", itemAdapter.getSelectedItemKey());

        // Remove the item after the currently selected item.
        mChooserDialog.removeItemFromList("key3");
        assertTrue(button.isEnabled());
        assertEquals("key2", itemAdapter.getSelectedItemKey());

        mChooserDialog.dismiss();
    }

    @LargeTest
    public void testSelectAnItemAndRemoveTheSelectedItem() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        final Button button = (Button) dialog.findViewById(R.id.positive);
        ItemChooserDialog.ItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

        mChooserDialog.addOrUpdateItem("key1", "desc1");
        mChooserDialog.addOrUpdateItem("key2", "desc2");
        mChooserDialog.addOrUpdateItem("key3", "desc3");

        selectItem(dialog, 2, "key2", true);
        assertTrue(button.isEnabled());

        // Remove the selected item.
        mChooserDialog.removeItemFromList("key2");
        assertFalse(button.isEnabled());
        assertEquals("", itemAdapter.getSelectedItemKey());

        mChooserDialog.dismiss();
    }

    @LargeTest
    @UiThreadTest
    public void testUpdateItemAndRemoveItemFromList() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = (TextViewWithClickableSpans)
                dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        ItemChooserDialog.ItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();
        final String nonExistentKey = "key";

        // Initially the itemAdapter is empty.
        assertTrue(itemAdapter.isEmpty());

        // Try removing an item from an empty itemAdapter.
        mChooserDialog.removeItemFromList(nonExistentKey);
        assertTrue(itemAdapter.isEmpty());

        // Add item 1.
        mChooserDialog.addOrUpdateItem("key1", "desc1");
        assertEquals(1, itemAdapter.getCount());
        assertTrue(itemAdapter.getItem(0).hasSameContents("key1", "desc1"));

        // Update item 1 with different description.
        mChooserDialog.addOrUpdateItem("key1", "desc2");
        assertEquals(1, itemAdapter.getCount());
        assertTrue(itemAdapter.getItem(0).hasSameContents("key1", "desc2"));

        mChooserDialog.setIdleState();

        // Remove item 1.
        mChooserDialog.removeItemFromList("key1");
        assertTrue(itemAdapter.isEmpty());

        // Listview should now be showing empty, with an empty view visible
        // and the button should not be enabled.
        // The chooser should show a status message at the bottom.
        assertEquals(View.GONE, items.getVisibility());
        assertEquals(View.VISIBLE, items.getEmptyView().getVisibility());
        assertEquals("statusIdleNoneFound", statusView.getText().toString());
        assertFalse(button.isEnabled());

        mChooserDialog.dismiss();
    }

    @LargeTest
    @UiThreadTest
    public void testAddItemAndRemoveItemFromList() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        ItemChooserDialog.ItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();
        final String nonExistentKey = "key";

        // Initially the itemAdapter is empty.
        assertTrue(itemAdapter.isEmpty());

        // Try removing an item from an empty itemAdapter.
        mChooserDialog.removeItemFromList(nonExistentKey);
        assertTrue(itemAdapter.isEmpty());

        // Add item 1.
        mChooserDialog.addOrUpdateItem("key1", "desc1");
        assertEquals(1, itemAdapter.getCount());
        assertTrue(itemAdapter.getItem(0).hasSameContents("key1", "desc1"));

        // Add item 2.
        mChooserDialog.addOrUpdateItem("key2", "desc2");
        assertEquals(2, itemAdapter.getCount());
        assertTrue(itemAdapter.getItem(0).hasSameContents("key1", "desc1"));
        assertTrue(itemAdapter.getItem(1).hasSameContents("key2", "desc2"));

        mChooserDialog.setIdleState();

        // Try removing an item that doesn't exist.
        mChooserDialog.removeItemFromList(nonExistentKey);
        assertEquals(2, itemAdapter.getCount());

        // Remove item 2.
        mChooserDialog.removeItemFromList("key2");
        assertEquals(1, itemAdapter.getCount());
        assertTrue(itemAdapter.getItem(0).hasSameContents("key1", "desc1"));

        // The list should be visible with one item, it should not show
        // the empty view and the button should not be enabled.
        // The chooser should show a status message at the bottom.
        assertEquals(View.VISIBLE, items.getVisibility());
        assertEquals(View.GONE, items.getEmptyView().getVisibility());
        assertEquals("statusIdleSomeFound", statusView.getText().toString());
        assertFalse(button.isEnabled());

        // Remove item 1.
        mChooserDialog.removeItemFromList("key1");
        assertTrue(itemAdapter.isEmpty());

        // Listview should now be showing empty, with an empty view visible
        // and the button should not be enabled.
        // The chooser should show a status message at the bottom.
        assertEquals(View.GONE, items.getVisibility());
        assertEquals(View.VISIBLE, items.getEmptyView().getVisibility());
        assertEquals("statusIdleNoneFound", statusView.getText().toString());
        assertFalse(button.isEnabled());

        mChooserDialog.dismiss();
    }

    @LargeTest
    @UiThreadTest
    public void testAddItemWithSameNameToListAndRemoveItemFromList() {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        ItemChooserDialog.ItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

        // Add item 1.
        mChooserDialog.addOrUpdateItem("key1", "desc1");
        assertEquals(1, itemAdapter.getCount());
        // Add item 2.
        mChooserDialog.addOrUpdateItem("key2", "desc2");
        assertEquals(2, itemAdapter.getCount());
        // Add item 3 with same description as item 1.
        mChooserDialog.addOrUpdateItem("key3", "desc1");
        assertEquals(3, itemAdapter.getCount());
        assertTrue(itemAdapter.getItem(0).hasSameContents("key1", "desc1"));
        assertTrue(itemAdapter.getItem(1).hasSameContents("key2", "desc2"));
        assertTrue(itemAdapter.getItem(2).hasSameContents("key3", "desc1"));

        // Since two items have the same name, their display text should have their unique
        // keys appended.
        assertEquals("desc1 (key1)", itemAdapter.getDisplayText(0));
        assertEquals("desc2", itemAdapter.getDisplayText(1));
        assertEquals("desc1 (key3)", itemAdapter.getDisplayText(2));

        // Remove item 2.
        mChooserDialog.removeItemFromList("key2");
        assertEquals(2, itemAdapter.getCount());
        // Make sure the remaining items are item 1 and item 3.
        assertTrue(itemAdapter.getItem(0).hasSameContents("key1", "desc1"));
        assertTrue(itemAdapter.getItem(1).hasSameContents("key3", "desc1"));
        assertEquals("desc1 (key1)", itemAdapter.getDisplayText(0));
        assertEquals("desc1 (key3)", itemAdapter.getDisplayText(1));

        // Remove item 1.
        mChooserDialog.removeItemFromList("key1");
        assertEquals(1, itemAdapter.getCount());
        // Make sure the remaining item is item 3.
        assertTrue(itemAdapter.getItem(0).hasSameContents("key3", "desc1"));
        // After removing item 1, item 3 is the only remaining item, so its display text
        // also changed to its original description.
        assertEquals("desc1", itemAdapter.getDisplayText(0));

        mChooserDialog.dismiss();
    }

    @LargeTest
    public void testListHeight() {
        // 500 * .3 is 150, which is 48 * 3.125. 48 * 3.5 is 168.
        assertEquals(168, ItemChooserDialog.getListHeight(500, 1.0f));

        // 150 * .3 is 45, which rounds below the minimum height.
        assertEquals(72, ItemChooserDialog.getListHeight(150, 1.0f));

        // 1460 * .3 is 438, which rounds above the maximum height.
        assertEquals(408, ItemChooserDialog.getListHeight(1460, 1.0f));

        // 1100px is 500dp at a density of 2.2. 500 * .3 is 150dp, which is 48dp *
        // 3.125. 48dp * 3.5 is 168dp. 168dp * 2.2px/dp is 369.6, which rounds to
        // 370.
        assertEquals(370, ItemChooserDialog.getListHeight(1100, 2.2f));
    }
}
