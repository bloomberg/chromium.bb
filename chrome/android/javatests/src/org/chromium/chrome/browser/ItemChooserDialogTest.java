// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Dialog;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.SpannableString;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.FlakyTest;
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
            boolean expectedEnabledState) throws InterruptedException {
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

    @SmallTest
    public void testSimpleItemSelection() throws InterruptedException {
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

        mChooserDialog.addOrUpdateItem(new ItemChooserDialog.ItemChooserRow("key", "key"));
        mChooserDialog.addOrUpdateItem(new ItemChooserDialog.ItemChooserRow("key2", "key2"));

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
        selectItem(dialog, 1, "key", true);

        mChooserDialog.dismiss();
    }

    @SmallTest
    public void testNoItemsAddedDiscoveryIdle() throws InterruptedException {
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

    @SmallTest
    public void testDisabledSelection() throws InterruptedException {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        mChooserDialog.addOrUpdateItem(new ItemChooserDialog.ItemChooserRow("key", "key"));
        mChooserDialog.addOrUpdateItem(new ItemChooserDialog.ItemChooserRow("key2", "key2"));

        // Disable one item and try to select it.
        mChooserDialog.setEnabled("key", false);
        selectItem(dialog, 1, "None", false);
        // The other is still selectable.
        selectItem(dialog, 2, "key2", true);

        mChooserDialog.dismiss();
    }

    @SmallTest
    public void testAddOrUpdateItemAndRemoveItemFromList() throws InterruptedException {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView = (TextViewWithClickableSpans)
                dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        ItemChooserDialog.ItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();
        ItemChooserDialog.ItemChooserRow nonExistentItem =
                new ItemChooserDialog.ItemChooserRow("key", "key");

        // Initially the itemAdapter is empty.
        assertTrue(itemAdapter.isEmpty());

        // Try removing an item from an empty itemAdapter.
        mChooserDialog.removeItemFromList(nonExistentItem);
        assertTrue(itemAdapter.isEmpty());

        // Add item 1.
        ItemChooserDialog.ItemChooserRow item1 =
                new ItemChooserDialog.ItemChooserRow("key1", "desc1");
        mChooserDialog.addOrUpdateItem(item1);
        assertEquals(1, itemAdapter.getCount());
        assertEquals(itemAdapter.getItem(0), item1);

        // Add item 1 with different description.
        ItemChooserDialog.ItemChooserRow item1_again =
                new ItemChooserDialog.ItemChooserRow("key1", "desc1_again");
        mChooserDialog.addOrUpdateItem(item1_again);
        assertEquals(1, itemAdapter.getCount());
        assertEquals(itemAdapter.getItem(0), item1_again);

        // Add item 2.
        ItemChooserDialog.ItemChooserRow item2 =
                new ItemChooserDialog.ItemChooserRow("key2", "desc2");
        mChooserDialog.addOrUpdateItem(item2);
        assertEquals(2, itemAdapter.getCount());
        assertEquals(itemAdapter.getItem(0), item1_again);
        assertEquals(itemAdapter.getItem(1), item2);

        mChooserDialog.setIdleState();

        // Try removing an item that doesn't exist.
        mChooserDialog.removeItemFromList(nonExistentItem);
        assertEquals(2, itemAdapter.getCount());

        // Remove item 2.
        mChooserDialog.removeItemFromList(item2);
        assertEquals(1, itemAdapter.getCount());
        assertEquals(itemAdapter.getItem(0), item1_again);

        // The list should be visible with one item, it should not show
        // the empty view and the button should not be enabled.
        // The chooser should show a status message at the bottom.
        assertEquals(View.VISIBLE, items.getVisibility());
        assertEquals(View.GONE, items.getEmptyView().getVisibility());
        assertEquals("statusIdleSomeFound", statusView.getText().toString());
        assertFalse(button.isEnabled());

        // Remove item 1.
        mChooserDialog.removeItemFromList(item1_again);
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

    @FlakyTest
//    @SmallTest
//    crbug.com/629579
    public void testAddItemWithSameNameToListAndRemoveItemFromList() throws InterruptedException {
        Dialog dialog = mChooserDialog.getDialogForTesting();
        assertTrue(dialog.isShowing());

        ItemChooserDialog.ItemAdapter itemAdapter = mChooserDialog.getItemAdapterForTesting();

        // Add item 1.
        ItemChooserDialog.ItemChooserRow item1 =
                new ItemChooserDialog.ItemChooserRow("device_id_1", "same_device_name");
        mChooserDialog.addOrUpdateItem(item1);
        assertEquals(1, itemAdapter.getCount());
        assertEquals(itemAdapter.getItem(0), item1);

        // Add item 2.
        ItemChooserDialog.ItemChooserRow item2 =
                new ItemChooserDialog.ItemChooserRow("device_id_2", "different_device_name");
        mChooserDialog.addOrUpdateItem(item2);
        assertEquals(2, itemAdapter.getCount());
        assertEquals(itemAdapter.getItem(0), item1);
        assertEquals(itemAdapter.getItem(1), item2);

        // Add item 3.
        ItemChooserDialog.ItemChooserRow item3 =
                new ItemChooserDialog.ItemChooserRow("device_id_3", "same_device_name");
        mChooserDialog.addOrUpdateItem(item3);
        assertEquals(3, itemAdapter.getCount());
        assertEquals(itemAdapter.getItem(0), item1);
        assertEquals(itemAdapter.getItem(1), item2);
        assertEquals(itemAdapter.getItem(2), item3);

        // Since two items have the same name, their display text should have their unique
        // keys appended.
        assertEquals("same_device_name (device_id_1)", itemAdapter.getDisplayText(0));
        assertEquals("different_device_name", itemAdapter.getDisplayText(1));
        assertEquals("same_device_name (device_id_3)", itemAdapter.getDisplayText(2));

        // Remove item 2.
        mChooserDialog.removeItemFromList(item2);
        assertEquals(2, itemAdapter.getCount());
        // Make sure the remaining items are item 1 and item 3.
        assertEquals(itemAdapter.getItem(0), item1);
        assertEquals(itemAdapter.getItem(1), item3);
        assertEquals("same_device_name (device_id_1)", itemAdapter.getDisplayText(0));
        assertEquals("same_device_name (device_id_3)", itemAdapter.getDisplayText(1));

        // Remove item 1.
        mChooserDialog.removeItemFromList(item1);
        assertEquals(1, itemAdapter.getCount());
        // Make sure the remaining item is item 3.
        assertEquals(itemAdapter.getItem(0), item3);
        // After removing item 1, item 3 is the only remaining item, so its display text
        // also changed to its original description.
        assertEquals("same_device_name", itemAdapter.getDisplayText(0));

        mChooserDialog.dismiss();
    }

    @SmallTest
    public void testListHeight() throws InterruptedException {
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
