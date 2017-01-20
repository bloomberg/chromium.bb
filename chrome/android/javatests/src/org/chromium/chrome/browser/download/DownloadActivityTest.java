// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;
import android.view.View;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.ui.DownloadHistoryAdapter;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemViewHolder;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper;
import org.chromium.chrome.browser.download.ui.DownloadItemView;
import org.chromium.chrome.browser.download.ui.DownloadManagerToolbar;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.download.ui.SpaceDisplay;
import org.chromium.chrome.browser.download.ui.StubbedProvider;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.List;

/**
 * Tests the DownloadActivity and the DownloadManagerUi.
 */
@Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
public class DownloadActivityTest extends BaseActivityInstrumentationTestCase<DownloadActivity> {

    private static class TestObserver extends RecyclerView.AdapterDataObserver
            implements SelectionObserver<DownloadHistoryItemWrapper>,
                    DownloadManagerUi.DownloadUiObserver, SpaceDisplay.Observer {
        public final CallbackHelper onChangedCallback = new CallbackHelper();
        public final CallbackHelper onSelectionCallback = new CallbackHelper();
        public final CallbackHelper onFilterCallback = new CallbackHelper();
        public final CallbackHelper onSpaceDisplayUpdatedCallback = new CallbackHelper();

        private List<DownloadHistoryItemWrapper> mOnSelectionItems;
        private Handler mHandler;

        public TestObserver() {
            mHandler = new Handler(Looper.getMainLooper());
        }

        @Override
        public void onChanged() {
            // To guarantee that all real Observers have had a chance to react to the event, post
            // the CallbackHelper.notifyCalled() call.
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    onChangedCallback.notifyCalled();
                }
            });
        }

        @Override
        public void onSelectionStateChange(List<DownloadHistoryItemWrapper> selectedItems) {
            mOnSelectionItems = selectedItems;
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    onSelectionCallback.notifyCalled();
                }
            });
        }

        @Override
        public void onFilterChanged(int filter) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    onFilterCallback.notifyCalled();
                }
            });
        }

        @Override
        public void onSpaceDisplayUpdated(SpaceDisplay display) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    onSpaceDisplayUpdatedCallback.notifyCalled();
                }
            });
        }

        @Override
        public void onManagerDestroyed() {
        }
    }

    private StubbedProvider mStubbedProvider;
    private TestObserver mAdapterObserver;
    private DownloadManagerUi mUi;
    private DownloadHistoryAdapter mAdapter;

    private RecyclerView mRecyclerView;
    private TextView mSpaceUsedDisplay;

    public DownloadActivityTest() {
        super(DownloadActivity.class);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mStubbedProvider = new StubbedProvider();
        DownloadManagerUi.setProviderForTests(mStubbedProvider);

        mAdapterObserver = new TestObserver();
        mStubbedProvider.getSelectionDelegate().addObserver(mAdapterObserver);

        startDownloadActivity();
        mUi = getActivity().getDownloadManagerUiForTests();
        mAdapter = mUi.getDownloadHistoryAdapterForTests();
        mAdapter.registerAdapterDataObserver(mAdapterObserver);

        mSpaceUsedDisplay =
                (TextView) getActivity().findViewById(R.id.size_downloaded);
        mRecyclerView = ((RecyclerView) getActivity().findViewById(R.id.recycler_view));

        mUi.getSpaceDisplayForTests().addObserverForTests(mAdapterObserver);
    }

    @MediumTest
    public void testSpaceDisplay() throws Exception {
        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Add a new item.
        int callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem updateItem = StubbedProvider.createDownloadItem(7, "20151021 07:28");
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAdapter.onDownloadItemCreated(updateItem);
            }
        });
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        assertEquals("6.50 GB downloaded", mSpaceUsedDisplay.getText());

        // Mark one download as deleted on disk, which should prevent it from being counted.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem deletedItem = StubbedProvider.createDownloadItem(6, "20151021 07:28");
        deletedItem.setHasBeenExternallyRemoved(true);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAdapter.onDownloadItemUpdated(deletedItem);
            }
        });
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        assertEquals("5.50 GB downloaded", mSpaceUsedDisplay.getText());

        // Say that the offline page has been deleted.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final OfflinePageDownloadItem deletedPage =
                StubbedProvider.createOfflineItem(3, "20151021 07:28");
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mStubbedProvider.getOfflinePageBridge().observer.onItemDeleted(
                        deletedPage.getGuid());
            }
        });
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        assertEquals("512.00 MB downloaded", mSpaceUsedDisplay.getText());
    }

    /** Clicking on filters affects various things in the UI. */
    @MediumTest
    public void testFilters() throws Exception {
        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Change the filter. Only the Offline Page and its date header should stay.
        int spaceDisplayCallCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        clickOnFilter(mUi, 1);
        assertEquals(2, mAdapter.getItemCount());

        // The title of the toolbar reflects the filter.
        assertEquals("Pages", mUi.getDownloadManagerToolbarForTests().getTitle());

        // Check that the number of items displayed is correct.
        // We need to poll because RecyclerView doesn't animate changes immediately.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mRecyclerView.getChildCount() == 2;
            }
        });

        // Filtering doesn't affect the total download size.
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(spaceDisplayCallCount);
        assertEquals("6.00 GB downloaded", mSpaceUsedDisplay.getText());
    }

    @MediumTest
    @RetryOnFailure
    public void testDeleteFiles() throws Exception {
        SnackbarManager.setDurationForTesting(1);

        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Select the first two items.
        toggleItemSelection(1);
        toggleItemSelection(2);

        // Click the delete button, which should delete the items and reset the toolbar.
        assertEquals(11, mAdapter.getItemCount());
        // checkForExternallyRemovedFiles() should have been called once already in onResume().
        assertEquals(1,
                mStubbedProvider.getDownloadDelegate().checkExternalCallback.getCallCount());
        assertEquals(0,
                mStubbedProvider.getDownloadDelegate().removeDownloadCallback.getCallCount());
        assertEquals(0, mStubbedProvider.getOfflinePageBridge().deleteItemCallback.getCallCount());
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(mUi.getDownloadManagerToolbarForTests().getMenu()
                        .performIdentifierAction(R.id.selection_mode_delete_menu_id, 0));
            }
        });

        mStubbedProvider.getDownloadDelegate().removeDownloadCallback.waitForCallback(0);
        assertEquals(1,
                mStubbedProvider.getDownloadDelegate().checkExternalCallback.getCallCount());
        mStubbedProvider.getOfflinePageBridge().deleteItemCallback.waitForCallback(0);
        assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        assertEquals(8, mAdapter.getItemCount());
        assertEquals("0.65 KB downloaded", mSpaceUsedDisplay.getText());
    }

    @MediumTest
    @RetryOnFailure
    public void testUndoDelete() throws Exception {
        // Adapter positions:
        // 0 = date
        // 1 = download item #7
        // 2 = download item #8
        // 3 = date
        // 4 = download item #6
        // 5 = offline page #3

        SnackbarManager.setDurationForTesting(5000);

        // Add duplicate items.
        int callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem item7 = StubbedProvider.createDownloadItem(7, "20161021 07:28");
        final DownloadItem item8 = StubbedProvider.createDownloadItem(8, "20161021 17:28");
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAdapter.onDownloadItemCreated(item7);
                mAdapter.onDownloadItemCreated(item8);
            }
        });

        // The criteria is needed because an AsyncTask is fired to update the space display, which
        // can result in either 1 or 2 updates.
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("7.00 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Select download item #7 and offline page #3.
        toggleItemSelection(1);
        toggleItemSelection(5);

        assertEquals(14, mAdapter.getItemCount());

        // Click the delete button.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(mUi.getDownloadManagerToolbarForTests().getMenu()
                        .performIdentifierAction(R.id.selection_mode_delete_menu_id, 0));
            }
        });
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that items are temporarily removed from the adapter. The two selected items,
        // one duplicate item, and one date bucket should be removed.
        assertEquals(10, mAdapter.getItemCount());
        assertEquals("1.00 GB downloaded", mSpaceUsedDisplay.getText());

        // Click "Undo" on the snackbar.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final View rootView = mUi.getView().getRootView();
        assertNotNull(rootView.findViewById(R.id.snackbar));
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                rootView.findViewById(R.id.snackbar_button).callOnClick();
            }
        });

        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that items are restored.
        assertEquals(0,
                mStubbedProvider.getDownloadDelegate().removeDownloadCallback.getCallCount());
        assertEquals(0,
                mStubbedProvider.getOfflinePageBridge().deleteItemCallback.getCallCount());
        assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        assertEquals(14, mAdapter.getItemCount());
        assertEquals("7.00 GB downloaded", mSpaceUsedDisplay.getText());
    }

    @MediumTest
    @RetryOnFailure
    public void testUndoDeleteDuplicatesSelected() throws Exception {
        // Adapter positions:
        // 0 = date
        // 1 = download item #7
        // 2 = download item #8
        // ....

        SnackbarManager.setDurationForTesting(5000);

        // Add duplicate items.
        int callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final DownloadItem item7 = StubbedProvider.createDownloadItem(7, "20161021 07:28");
        final DownloadItem item8 = StubbedProvider.createDownloadItem(8, "20161021 17:28");
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAdapter.onDownloadItemCreated(item7);
                mAdapter.onDownloadItemCreated(item8);
            }
        });

        // The criteria is needed because an AsyncTask is fired to update the space display, which
        // can result in either 1 or 2 updates.
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("7.00 GB downloaded", mSpaceUsedDisplay.getText());
            }
        });

        // Select download item #7 and download item #8.
        toggleItemSelection(1);
        toggleItemSelection(2);

        assertEquals(14, mAdapter.getItemCount());

        // Click the delete button.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(mUi.getDownloadManagerToolbarForTests().getMenu()
                        .performIdentifierAction(R.id.selection_mode_delete_menu_id, 0));
            }
        });
        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that the two items and their date bucket are temporarily removed from the adapter.
        assertEquals(11, mAdapter.getItemCount());
        assertEquals("6.00 GB downloaded", mSpaceUsedDisplay.getText());

        // Click "Undo" on the snackbar.
        callCount = mAdapterObserver.onSpaceDisplayUpdatedCallback.getCallCount();
        final View rootView = mUi.getView().getRootView();
        assertNotNull(rootView.findViewById(R.id.snackbar));
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                rootView.findViewById(R.id.snackbar_button).callOnClick();
            }
        });

        mAdapterObserver.onSpaceDisplayUpdatedCallback.waitForCallback(callCount);

        // Assert that items are restored.
        assertEquals(14, mAdapter.getItemCount());
        assertEquals("7.00 GB downloaded", mSpaceUsedDisplay.getText());
    }

    @MediumTest
    public void testShareFiles() throws Exception {
        // Adapter positions:
        // 0 = date
        // 1 = download item #6
        // 2 = offline page #3
        // 3 = date
        // 4 = download item #3
        // 5 = download item #4
        // 6 = download item #5
        // 7 = date
        // 8 = download item #0
        // 9 = download item #1
        // 10 = download item #2

        // Select an image, download item #6.
        toggleItemSelection(1);
        Intent shareIntent = mUi.createShareIntent();
        assertEquals("Incorrect intent action", Intent.ACTION_SEND, shareIntent.getAction());
        assertEquals("Incorrect intent mime type", "image/png", shareIntent.getType());
        assertNotNull("Intent expected to have stream",
                shareIntent.getExtras().get(Intent.EXTRA_STREAM));
        assertNull("Intent not expected to have parcelable ArrayList",
                shareIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM));

        // Scroll to ensure the item at position 8 is visible.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mRecyclerView.scrollToPosition(8);
            }
        });
        getInstrumentation().waitForIdleSync();

        // Select another image, download item #0.
        toggleItemSelection(8);
        shareIntent = mUi.createShareIntent();
        assertEquals("Incorrect intent action", Intent.ACTION_SEND_MULTIPLE,
                shareIntent.getAction());
        assertEquals("Incorrect intent mime type", "image/*", shareIntent.getType());
        assertEquals("Intent expected to have parcelable ArrayList",
                2, shareIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM).size());

        // Scroll to ensure the item at position 5 is visible.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mRecyclerView.scrollToPosition(5);
            }
        });
        getInstrumentation().waitForIdleSync();

        // Select non-image item, download item #4.
        toggleItemSelection(5);
        shareIntent = mUi.createShareIntent();
        assertEquals("Incorrect intent action", Intent.ACTION_SEND_MULTIPLE,
                shareIntent.getAction());
        assertEquals("Incorrect intent mime type", "*/*", shareIntent.getType());
        assertEquals("Intent expected to have parcelable ArrayList",
                3, shareIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM).size());

        // Scroll to ensure the item at position 2 is visible.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mRecyclerView.scrollToPosition(2);
            }
        });
        getInstrumentation().waitForIdleSync();

        // Select an offline page #3.
        toggleItemSelection(2);
        shareIntent = mUi.createShareIntent();
        assertEquals("Incorrect intent action", Intent.ACTION_SEND_MULTIPLE,
                shareIntent.getAction());
        assertEquals("Incorrect intent mime type", "*/*", shareIntent.getType());
        assertEquals("Intent expected to have parcelable ArrayList",
                3, shareIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM).size());
        assertEquals("Intent expected to have plain text for offline page URL",
                "https://thangs.com",
                IntentUtils.safeGetStringExtra(shareIntent, Intent.EXTRA_TEXT));
    }

    @MediumTest
    public void testToggleSelection() throws Exception {
        // The selection toolbar should not be showing.
        assertTrue(mAdapterObserver.mOnSelectionItems.isEmpty());
        assertEquals(View.VISIBLE, getActivity().findViewById(R.id.close_menu_id).getVisibility());
        assertEquals(View.GONE,
                getActivity().findViewById(R.id.selection_mode_number).getVisibility());
        assertNull(getActivity().findViewById(R.id.selection_mode_share_menu_id));
        assertNull(getActivity().findViewById(R.id.selection_mode_delete_menu_id));
        assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());

        // Select an item.
        toggleItemSelection(1);

        // The toolbar should flip states to allow doing things with the selected items.
        assertNull(getActivity().findViewById(R.id.close_menu_id));
        assertEquals(View.VISIBLE,
                getActivity().findViewById(R.id.selection_mode_number).getVisibility());
        assertEquals(View.VISIBLE,
                getActivity().findViewById(R.id.selection_mode_share_menu_id).getVisibility());
        assertEquals(View.VISIBLE,
                getActivity().findViewById(R.id.selection_mode_delete_menu_id).getVisibility());
        assertTrue(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());

        // Deselect the same item.
        toggleItemSelection(1);

        // The toolbar should flip back.
        assertTrue(mAdapterObserver.mOnSelectionItems.isEmpty());
        assertEquals(View.VISIBLE, getActivity().findViewById(R.id.close_menu_id).getVisibility());
        assertEquals(View.GONE,
                getActivity().findViewById(R.id.selection_mode_number).getVisibility());
        assertNull(getActivity().findViewById(R.id.selection_mode_share_menu_id));
        assertNull(getActivity().findViewById(R.id.selection_mode_delete_menu_id));
        assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
    }

    @MediumTest
    public void testSearchView() throws Exception {
        final DownloadManagerToolbar toolbar = mUi.getDownloadManagerToolbarForTests();
        View toolbarSearchView = toolbar.getSearchViewForTests();
        assertEquals(View.GONE, toolbarSearchView.getVisibility());

        toggleItemSelection(2);
        assertTrue(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());

        int callCount = mAdapterObserver.onSelectionCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                toolbar.getMenu().performIdentifierAction(R.id.search_menu_id, 0);
            }
        });

        // The selection should be cleared when a search is started.
        mAdapterObserver.onSelectionCallback.waitForCallback(callCount, 1);
        assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());

        // Select an item and assert that the search view is no longer showing.
        toggleItemSelection(2);
        assertTrue(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        assertEquals(View.GONE, toolbarSearchView.getVisibility());

        // Clear the selection and assert that the search view is showing again.
        toggleItemSelection(2);
        assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());

        // Close the search view.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                toolbar.onNavigationBack();
            }
        });
        assertEquals(View.GONE, toolbarSearchView.getVisibility());
    }

    private DownloadActivity startDownloadActivity() throws Exception {
        // Load up the downloads lists.
        DownloadItem item0 = StubbedProvider.createDownloadItem(0, "19551112 06:38");
        DownloadItem item1 = StubbedProvider.createDownloadItem(1, "19551112 06:38");
        DownloadItem item2 = StubbedProvider.createDownloadItem(2, "19551112 06:38");
        DownloadItem item3 = StubbedProvider.createDownloadItem(3, "19851026 09:00");
        DownloadItem item4 = StubbedProvider.createDownloadItem(4, "19851026 09:00");
        DownloadItem item5 = StubbedProvider.createDownloadItem(5, "19851026 09:00");
        DownloadItem item6 = StubbedProvider.createDownloadItem(6, "20151021 07:28");
        OfflinePageDownloadItem item7 = StubbedProvider.createOfflineItem(3, "20151021 07:28");
        mStubbedProvider.getDownloadDelegate().regularItems.add(item0);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item1);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item2);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item3);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item4);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item5);
        mStubbedProvider.getDownloadDelegate().regularItems.add(item6);
        mStubbedProvider.getOfflinePageBridge().items.add(item7);

        // Start the activity up.
        Intent intent = new Intent();
        intent.setClass(getInstrumentation().getTargetContext(), DownloadActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        setActivityIntent(intent);
        return getActivity();
    }

    private void clickOnFilter(final DownloadManagerUi ui, final int position) throws Exception {
        int previousCount = mAdapterObserver.onChangedCallback.getCallCount();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ui.openDrawer();
                ListView filterList = (ListView) ui.getView().findViewById(R.id.section_list);
                filterList.performItemClick(
                        filterList.getChildAt(filterList.getHeaderViewsCount() + position),
                        position, filterList.getAdapter().getItemId(position));
            }
        });
        mAdapterObserver.onChangedCallback.waitForCallback(previousCount);
    }

    private void toggleItemSelection(int position) throws Exception {
        int callCount = mAdapterObserver.onSelectionCallback.getCallCount();
        ViewHolder mostRecentHolder = mRecyclerView.findViewHolderForAdapterPosition(position);
        assertTrue(mostRecentHolder instanceof DownloadHistoryItemViewHolder);
        final DownloadItemView itemView =
                ((DownloadHistoryItemViewHolder) mostRecentHolder).getItemView();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                itemView.performLongClick();
            }
        });
        mAdapterObserver.onSelectionCallback.waitForCallback(callCount, 1);
    }
}
