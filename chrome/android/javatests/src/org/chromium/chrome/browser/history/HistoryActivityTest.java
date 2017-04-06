// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.accounts.Account;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;
import android.os.Looper;
import android.os.PatternMatcher;
import android.provider.Browser;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.childaccounts.ChildAccountService;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;
import org.chromium.chrome.browser.widget.selection.SelectableItemViewHolder;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.Date;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/**
 * Tests the {@link HistoryActivity}.
 */
@Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
public class HistoryActivityTest extends BaseActivityInstrumentationTestCase<HistoryActivity> {
    private static class TestObserver extends RecyclerView.AdapterDataObserver
            implements SelectionObserver<HistoryItem>, SignInStateObserver {
        public final CallbackHelper onChangedCallback = new CallbackHelper();
        public final CallbackHelper onSelectionCallback = new CallbackHelper();
        public final CallbackHelper onSigninStateChangedCallback = new CallbackHelper();

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
        public void onSelectionStateChange(List<HistoryItem> selectedItems) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    onSelectionCallback.notifyCalled();
                }
            });
        }

        @Override
        public void onSignedIn() {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    onSigninStateChangedCallback.notifyCalled();
                }
            });
        }

        @Override
        public void onSignedOut() {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    onSigninStateChangedCallback.notifyCalled();
                }
            });
        }
    }

    private StubbedHistoryProvider mHistoryProvider;
    private HistoryAdapter mAdapter;
    private HistoryManager mHistoryManager;
    private RecyclerView mRecyclerView;
    private TestObserver mTestObserver;

    private HistoryItem mItem1;
    private HistoryItem mItem2;

    public HistoryActivityTest() {
        super(HistoryActivity.class);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mHistoryProvider = new StubbedHistoryProvider();

        Date today = new Date();
        long[] timestamps = {today.getTime()};
        mItem1 = StubbedHistoryProvider.createHistoryItem(0, timestamps);
        mItem2 = StubbedHistoryProvider.createHistoryItem(1, timestamps);
        mHistoryProvider.addItem(mItem1);
        mHistoryProvider.addItem(mItem2);

        HistoryManager.setProviderForTests(mHistoryProvider);

        final HistoryActivity activity = getActivity();
        mHistoryManager = activity.getHistoryManagerForTests();
        mAdapter = mHistoryManager.getAdapterForTests();
        mTestObserver = new TestObserver();
        mHistoryManager.getSelectionDelegateForTests().addObserver(mTestObserver);
        mAdapter.registerAdapterDataObserver(mTestObserver);
        mRecyclerView = ((RecyclerView) activity.findViewById(R.id.recycler_view));

        assertEquals(4, mAdapter.getItemCount());
    }

    @SmallTest
    public void testRemove_SingleItem() throws Exception {
        int callCount = mTestObserver.onChangedCallback.getCallCount();
        final SelectableItemView<HistoryItem> itemView = getItemView(2);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((TintedImageButton) itemView.findViewById(R.id.remove)).performClick();
            }
        });

        // Check that one item was removed.
        mTestObserver.onChangedCallback.waitForCallback(callCount, 1);
        assertEquals(1, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
        assertEquals(3, mAdapter.getItemCount());
        assertEquals(View.VISIBLE, mRecyclerView.getVisibility());
        assertEquals(View.GONE, mHistoryManager.getEmptyViewForTests().getVisibility());
    }

    @SmallTest
    public void testRemove_AllItems() throws Exception {
        toggleItemSelection(2);
        toggleItemSelection(3);

        int callCount = mTestObserver.onChangedCallback.getCallCount();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(mHistoryManager.getToolbarForTests().getMenu()
                        .performIdentifierAction(R.id.selection_mode_delete_menu_id, 0));
            }
        });

        // Check that all items were removed. The onChangedCallback should be called three times -
        // once for each item that is being removed and once for the removal of the header.
        mTestObserver.onChangedCallback.waitForCallback(callCount, 3);
        assertEquals(0, mAdapter.getItemCount());
        assertEquals(2, mHistoryProvider.markItemForRemovalCallback.getCallCount());
        assertEquals(1, mHistoryProvider.removeItemsCallback.getCallCount());
        assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.GONE, mRecyclerView.getVisibility());
        assertEquals(View.VISIBLE, mHistoryManager.getEmptyViewForTests().getVisibility());
    }

    @SmallTest
    public void testPrivacyDisclaimers_SignedOut() {
        ChromeSigninController signinController = ChromeSigninController.get(getActivity());
        signinController.setSignedInAccountName(null);

        assertEquals(View.GONE, mAdapter.getSignedInNotSyncedViewForTests().getVisibility());
        assertEquals(View.GONE, mAdapter.getSignedInSyncedViewForTests().getVisibility());
        assertEquals(View.GONE,
                mAdapter.getOtherFormsOfBrowsingHistoryViewForTests().getVisibility());
    }

    @SmallTest
    public void testPrivacyDisclaimers_SignedIn() {
        ChromeSigninController signinController = ChromeSigninController.get(getActivity());
        signinController.setSignedInAccountName("test@gmail.com");

        setHasOtherFormsOfBrowsingData(false, false);

        assertEquals(View.VISIBLE, mAdapter.getSignedInNotSyncedViewForTests().getVisibility());
        assertEquals(View.GONE, mAdapter.getSignedInSyncedViewForTests().getVisibility());
        assertEquals(View.GONE,
                mAdapter.getOtherFormsOfBrowsingHistoryViewForTests().getVisibility());

        signinController.setSignedInAccountName(null);
    }

    @SmallTest
    public void testPrivacyDisclaimers_SignedInSynced() {
        ChromeSigninController signinController = ChromeSigninController.get(getActivity());
        signinController.setSignedInAccountName("test@gmail.com");

        setHasOtherFormsOfBrowsingData(false, true);

        assertEquals(View.GONE, mAdapter.getSignedInNotSyncedViewForTests().getVisibility());
        assertEquals(View.VISIBLE, mAdapter.getSignedInSyncedViewForTests().getVisibility());
        assertEquals(View.GONE,
                mAdapter.getOtherFormsOfBrowsingHistoryViewForTests().getVisibility());

        signinController.setSignedInAccountName(null);
    }

    @SmallTest
    public void testPrivacyDisclaimers_SignedInSyncedAndOtherForms() {
        ChromeSigninController signinController = ChromeSigninController.get(getActivity());
        signinController.setSignedInAccountName("test@gmail.com");

        setHasOtherFormsOfBrowsingData(true, true);

        assertEquals(View.GONE, mAdapter.getSignedInNotSyncedViewForTests().getVisibility());
        assertEquals(View.VISIBLE, mAdapter.getSignedInSyncedViewForTests().getVisibility());
        assertEquals(View.VISIBLE,
                mAdapter.getOtherFormsOfBrowsingHistoryViewForTests().getVisibility());

        signinController.setSignedInAccountName(null);
    }

    @SmallTest
    public void testOpenItem() throws Exception {
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataPath(mItem1.getUrl(), PatternMatcher.PATTERN_LITERAL);
        final ActivityMonitor activityMonitor = getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                true);

        clickItem(2);

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInstrumentation().checkMonitorHit(activityMonitor, 1);
            }
        });
    }

    @SmallTest
    public void testOpenSelectedItems() throws Exception {
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataPath(mItem1.getUrl(), PatternMatcher.PATTERN_LITERAL);
        filter.addDataPath(mItem2.getUrl(), PatternMatcher.PATTERN_LITERAL);
        final ActivityMonitor activityMonitor = getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                true);

        toggleItemSelection(2);
        toggleItemSelection(3);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertTrue(mHistoryManager.getToolbarForTests().getMenu()
                        .performIdentifierAction(R.id.selection_mode_open_in_incognito, 0));
            }
        });

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getInstrumentation().checkMonitorHit(activityMonitor, 2);
            }
        });
    }

    @SmallTest
    public void testOpenItemIntent() {
        Intent intent = mHistoryManager.getOpenUrlIntent(mItem1.getUrl(), null, false);
        assertEquals(mItem1.getUrl(), intent.getDataString());
        assertFalse(intent.hasExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB));
        assertFalse(intent.hasExtra(Browser.EXTRA_CREATE_NEW_TAB));

        intent = mHistoryManager.getOpenUrlIntent(mItem2.getUrl(), true, true);
        assertEquals(mItem2.getUrl(), intent.getDataString());
        assertTrue(intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
        assertTrue(intent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
    }

    @SmallTest
    public void testOnHistoryDeleted() throws Exception {
        toggleItemSelection(2);

        mHistoryProvider.removeItem(mItem1);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.onHistoryDeleted();
            }
        });

        // The selection should be cleared and the items in the adapter should be reloaded.
        assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(3, mAdapter.getItemCount());
    }

    @SmallTest
    public void testSupervisedUser() throws Exception {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        SelectableItemView<HistoryItem> item = getItemView(2);
        View itemRemoveButton = item.findViewById(R.id.remove);

        // The item's remove button is visible for non-supervised users when there is no selection.
        assertEquals(View.VISIBLE, itemRemoveButton.getVisibility());

        toggleItemSelection(2);
        assertTrue(toolbar.getItemById(R.id.selection_mode_open_in_incognito).isVisible());
        assertTrue(toolbar.getItemById(R.id.selection_mode_open_in_incognito).isEnabled());
        assertTrue(toolbar.getItemById(R.id.selection_mode_delete_menu_id).isVisible());
        assertTrue(toolbar.getItemById(R.id.selection_mode_delete_menu_id).isEnabled());
        // The item's remove button is invisible for non-supervised users when there is a selection.
        assertEquals(View.INVISIBLE, item.findViewById(R.id.remove).getVisibility());

        signInToSupervisedAccount();

        assertNull(toolbar.getItemById(R.id.selection_mode_open_in_incognito));
        assertNull(toolbar.getItemById(R.id.selection_mode_delete_menu_id));
        // The item's remove button should be gone for supervised users.
        assertEquals(View.GONE, item.findViewById(R.id.remove).getVisibility());

        // The item's remove button visibility is updated when toggling selection.
        // Check that the visibility is set correctly when toggling selection.
        toggleItemSelection(2);
        assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.GONE, item.findViewById(R.id.remove).getVisibility());

        toggleItemSelection(2);
        assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.GONE, item.findViewById(R.id.remove).getVisibility());

        signOut();

        // Check that the item's remove button visibilty is set correctly after signing out.
        assertEquals(View.INVISIBLE, item.findViewById(R.id.remove).getVisibility());
        toggleItemSelection(2);
        assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.VISIBLE, item.findViewById(R.id.remove).getVisibility());
    }

    @SmallTest
    public void testToolbarShadow() throws Exception {
        View toolbarShadow = mHistoryManager.getSelectableListLayout().getToolbarShadowForTests();
        assertEquals(View.GONE, toolbarShadow.getVisibility());

        toggleItemSelection(2);
        assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.VISIBLE, toolbarShadow.getVisibility());

        toggleItemSelection(2);
        assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.GONE, toolbarShadow.getVisibility());
    }

    @SmallTest
    public void testSearchView() throws Exception {
        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();
        View toolbarShadow = mHistoryManager.getSelectableListLayout().getToolbarShadowForTests();
        View toolbarSearchView = toolbar.getSearchViewForTests();
        assertEquals(View.GONE, toolbarShadow.getVisibility());
        assertEquals(View.GONE, toolbarSearchView.getVisibility());

        toggleItemSelection(2);
        assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());

        int callCount = mTestObserver.onSelectionCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                toolbar.getMenu().performIdentifierAction(R.id.search_menu_id, 0);
            }
        });

        // The selection should be cleared when a search is started.
        mTestObserver.onSelectionCallback.waitForCallback(callCount, 1);
        assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.VISIBLE, toolbarShadow.getVisibility());
        assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());

        // Select an item and assert that the search view is no longer showing.
        toggleItemSelection(2);
        assertTrue(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.VISIBLE, toolbarShadow.getVisibility());
        assertEquals(View.GONE, toolbarSearchView.getVisibility());

        // Clear the selection and assert that the search view is showing again.
        toggleItemSelection(2);
        assertFalse(mHistoryManager.getSelectionDelegateForTests().isSelectionEnabled());
        assertEquals(View.VISIBLE, toolbarShadow.getVisibility());
        assertEquals(View.VISIBLE, toolbarSearchView.getVisibility());

        // Close the search view.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                toolbar.onNavigationBack();
            }
        });
        assertEquals(View.GONE, toolbarShadow.getVisibility());
        assertEquals(View.GONE, toolbarSearchView.getVisibility());
    }

    @SmallTest
    public void testCopyLink() throws Exception {
        final ClipboardManager clipboardManager = (ClipboardManager)
                getActivity().getSystemService(getActivity().CLIPBOARD_SERVICE);
        assertNotNull(clipboardManager);
        // Clear the clipboard to make sure we start with a clean state.
        clipboardManager.setPrimaryClip(ClipData.newPlainText(null, ""));

        final HistoryManagerToolbar toolbar = mHistoryManager.getToolbarForTests();

        // Check that the copy link item is visible when one item is selected.
        toggleItemSelection(2);
        assertTrue(toolbar.getItemById(R.id.selection_mode_copy_link).isVisible());

        // Check that link is copied to the clipboard.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(mHistoryManager.getToolbarForTests().getMenu()
                        .performIdentifierAction(R.id.selection_mode_copy_link, 0));
            }
        });
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals(mItem1.getUrl(), clipboardManager.getText());
            }
        });

        // Check that the copy link item is not visible when more than one item is selected.
        toggleItemSelection(2);
        toggleItemSelection(3);
        assertFalse(toolbar.getItemById(R.id.selection_mode_copy_link).isVisible());
    }

    private void toggleItemSelection(int position) throws Exception {
        int callCount = mTestObserver.onSelectionCallback.getCallCount();
        final SelectableItemView<HistoryItem> itemView = getItemView(position);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                itemView.performLongClick();
            }
        });
        mTestObserver.onSelectionCallback.waitForCallback(callCount, 1);
    }

    private void clickItem(int position) throws Exception {
        final SelectableItemView<HistoryItem> itemView = getItemView(position);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                itemView.performClick();
            }
        });
    }

    @SuppressWarnings("unchecked")
    private SelectableItemView<HistoryItem> getItemView(int position) {
        ViewHolder mostRecentHolder = mRecyclerView.findViewHolderForAdapterPosition(position);
        assertTrue(mostRecentHolder instanceof SelectableItemViewHolder);
        return ((SelectableItemViewHolder<HistoryItem>) mostRecentHolder).getItemView();
    }

    private void setHasOtherFormsOfBrowsingData(final boolean hasOtherForms,
            final boolean hasSyncedResults)  {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAdapter.hasOtherFormsOfBrowsingData(hasOtherForms, hasSyncedResults);
            }
        });
    }

    private void signInToSupervisedAccount() throws Exception {
        // Set supervised user.
        assertTrue(ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                PrefServiceBridge.getInstance().setSupervisedUserId("ChildAccountSUID");
                return ChildAccountService.isChildAccount()
                        && !PrefServiceBridge.getInstance().canDeleteBrowsingHistory()
                        && !PrefServiceBridge.getInstance().isIncognitoModeEnabled();
            }
        }));

        // Sign in to account.
        SigninTestUtil.setUpAuthForTest(getInstrumentation());
        final Account account = SigninTestUtil.addTestAccount();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(getActivity()).onFirstRunCheckDone();
                SigninManager.get(getActivity()).addSignInStateObserver(mTestObserver);
                SigninManager.get(getActivity()).signIn(account, null, null);
            }
        });
        mTestObserver.onSigninStateChangedCallback.waitForCallback(0, 1,
                SyncTestUtil.TIMEOUT_MS, TimeUnit.MILLISECONDS);
        assertEquals(account, SigninTestUtil.getCurrentAccount());
    }

    private void signOut() throws Exception {
        // Clear supervised user id.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setSupervisedUserId("");
            }
        });

        // Sign out of account.
        int currentCallCount = mTestObserver.onSigninStateChangedCallback.getCallCount();
        SigninTestUtil.resetSigninState();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(getActivity()).signOut(null);
            }
        });
        mTestObserver.onSigninStateChangedCallback.waitForCallback(currentCallCount, 1);
        assertNull(SigninTestUtil.getCurrentAccount());

        // Remove observer
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(getActivity()).removeSignInStateObserver(mTestObserver);
            }
        });
    }
}
