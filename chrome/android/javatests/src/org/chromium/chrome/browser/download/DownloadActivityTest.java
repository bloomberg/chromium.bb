// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;
import android.view.View;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.ui.DownloadHistoryAdapter;
import org.chromium.chrome.browser.download.ui.DownloadHistoryAdapter.ItemViewHolder;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper;
import org.chromium.chrome.browser.download.ui.DownloadItemView;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.download.ui.StubbedProvider;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadItem;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.content.browser.test.util.CallbackHelper;
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
                    DownloadManagerUi.DownloadUiObserver {
        public final CallbackHelper onChangedCallback = new CallbackHelper();
        public final CallbackHelper onSelectionCallback = new CallbackHelper();
        public final CallbackHelper onFilterCallback = new CallbackHelper();

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
                (TextView) getActivity().findViewById(R.id.space_used_display);
        mRecyclerView = ((RecyclerView) getActivity().findViewById(R.id.recycler_view));
    }

    @MediumTest
    public void testSpaceDisplay() throws Exception {
        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB used", mSpaceUsedDisplay.getText());
            }
        });

        // Add a new item.
        int callCount = mAdapterObserver.onChangedCallback.getCallCount();
        final DownloadItem updateItem = StubbedProvider.createDownloadItem(7, "20151021 07:28");
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAdapter.onDownloadItemUpdated(updateItem, false);
            }
        });
        mAdapterObserver.onChangedCallback.waitForCallback(callCount);
        assertEquals("6.50 GB used", mSpaceUsedDisplay.getText());

        // Mark one download as deleted on disk, which should prevent it from being counted.
        callCount = mAdapterObserver.onChangedCallback.getCallCount();
        final DownloadItem deletedItem = StubbedProvider.createDownloadItem(6, "20151021 07:28");
        deletedItem.setHasBeenExternallyRemoved(true);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAdapter.onDownloadItemUpdated(deletedItem, false);
            }
        });
        mAdapterObserver.onChangedCallback.waitForCallback(callCount);
        assertEquals("5.50 GB used", mSpaceUsedDisplay.getText());

        // Say that the offline page has been deleted.
        callCount = mAdapterObserver.onChangedCallback.getCallCount();
        final OfflinePageDownloadItem deletedPage =
                StubbedProvider.createOfflineItem(3, "20151021 07:28");
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mStubbedProvider.getOfflinePageBridge().observer.onItemDeleted(
                        deletedPage.getGuid());
            }
        });
        mAdapterObserver.onChangedCallback.waitForCallback(callCount);
        assertEquals("0.50 GB used", mSpaceUsedDisplay.getText());
    }

    /** Clicking on filters affects various things in the UI. */
    @MediumTest
    public void testFilters() throws Exception {
        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB used", mSpaceUsedDisplay.getText());
            }
        });

        // Change the filter. Only the Offline Page and its date header should stay.
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
        assertEquals("6.00 GB used", mSpaceUsedDisplay.getText());
    }

    @MediumTest
    public void testDeleteFiles() throws Exception {
        // This first check is a Criteria because initialization of the Adapter is asynchronous.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals("6.00 GB used", mSpaceUsedDisplay.getText());
            }
        });

        // Select the first two items.
        ViewHolder mostRecentHolder = mRecyclerView.findViewHolderForAdapterPosition(1);
        assertTrue(mostRecentHolder instanceof ItemViewHolder);
        final DownloadItemView firstItemView = ((ItemViewHolder) mostRecentHolder).mItemView;

        ViewHolder nextMostRecentHolder = mRecyclerView.findViewHolderForAdapterPosition(2);
        assertTrue(nextMostRecentHolder instanceof ItemViewHolder);
        final DownloadItemView secondItemView = ((ItemViewHolder) nextMostRecentHolder).mItemView;

        assertTrue(mAdapterObserver.mOnSelectionItems.isEmpty());
        int callCount = mAdapterObserver.onSelectionCallback.getCallCount();
        assertEquals(View.VISIBLE, getActivity().findViewById(R.id.close_menu_id).getVisibility());
        assertEquals(View.GONE,
                getActivity().findViewById(R.id.selection_mode_number).getVisibility());
        assertNull(getActivity().findViewById(R.id.selection_mode_share_menu_id));
        assertNull(getActivity().findViewById(R.id.selection_mode_delete_menu_id));
        assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                firstItemView.performLongClick();
                secondItemView.performLongClick();
            }
        });

        // The toolbar should flip states to allow doing things with the selected items.
        mAdapterObserver.onSelectionCallback.waitForCallback(callCount, 2);
        assertNull(getActivity().findViewById(R.id.close_menu_id));
        assertEquals(View.VISIBLE,
                getActivity().findViewById(R.id.selection_mode_number).getVisibility());
        assertEquals(View.VISIBLE,
                getActivity().findViewById(R.id.selection_mode_share_menu_id).getVisibility());
        assertEquals(View.VISIBLE,
                getActivity().findViewById(R.id.selection_mode_delete_menu_id).getVisibility());
        assertTrue(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());

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
        mStubbedProvider.getDownloadDelegate().checkExternalCallback.waitForCallback(0);
        mStubbedProvider.getOfflinePageBridge().deleteItemCallback.waitForCallback(0);
        assertFalse(mStubbedProvider.getSelectionDelegate().isSelectionEnabled());
        assertEquals(8, mAdapter.getItemCount());
        assertEquals("0.00 GB used", mSpaceUsedDisplay.getText());
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
}
