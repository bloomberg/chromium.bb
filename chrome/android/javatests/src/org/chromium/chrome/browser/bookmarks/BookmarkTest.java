// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.Adapter;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the bookmark manager.
 */
@RetryOnFailure
public class BookmarkTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public BookmarkTest() {
        super(ChromeActivity.class);
    }

    private static final String TEST_PAGE_URL_GOOGLE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_TITLE_GOOGLE = "The Google";
    private static final String TEST_PAGE_TITLE_GOOGLE2 = "Google";
    private static final String TEST_PAGE_URL_FOO = "/chrome/test/data/android/test.html";
    private static final String TEST_PAGE_TITLE_FOO = "Foo";

    private BookmarkModel mBookmarkModel;
    protected RecyclerView mItemsContainer;
    private String mTestPage;
    private String mTestPageFoo;
    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE_URL_GOOGLE);
        mTestPageFoo = mTestServer.getURL(TEST_PAGE_URL_FOO);
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBookmarkModel = new BookmarkModel(getActivity().getActivityTab().getProfile());
            }
        });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    private void openBookmarkManager() throws InterruptedException {
        if (DeviceFormFactor.isTablet()) {
            loadUrl(UrlConstants.BOOKMARKS_URL);
            mItemsContainer = (RecyclerView) getActivity().findViewById(R.id.recycler_view);
        } else {
            // phone
            BookmarkActivity activity = ActivityUtils.waitForActivity(getInstrumentation(),
                    BookmarkActivity.class, new MenuUtils.MenuActivityTrigger(
                            getInstrumentation(), getActivity(), R.id.all_bookmarks_menu_id));
            mItemsContainer = (RecyclerView) activity.findViewById(R.id.recycler_view);
        }
    }

    private boolean isItemPresentInBookmarkList(final String expectedTitle) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                for (int i = 0; i < mItemsContainer.getAdapter().getItemCount(); i++) {
                    BookmarkId item =
                            ((BookmarkItemsAdapter) mItemsContainer.getAdapter()).getItem(i);
                    if (item == null) continue;

                    String actualTitle = mBookmarkModel.getBookmarkTitle(item);
                    if (TextUtils.equals(actualTitle, expectedTitle)) {
                        return true;
                    }
                }
                return false;
            }
        });
    }

    @SmallTest
    public void testAddBookmark() throws InterruptedException {
        loadUrl(mTestPage);
        // Click star button to bookmark the curent tab.
        MenuUtils.invokeCustomMenuActionSync(getInstrumentation(), getActivity(),
                R.id.bookmark_this_page_id);
        // All actions with BookmarkModel needs to run on UI thread.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                long bookmarkIdLong = getActivity().getActivityTab().getUserBookmarkId();
                BookmarkId id = new BookmarkId(bookmarkIdLong, BookmarkType.NORMAL);
                assertTrue("The test page is not added as bookmark: ",
                        mBookmarkModel.doesBookmarkExist(id));
                BookmarkItem item = mBookmarkModel.getBookmarkById(id);
                assertEquals(mBookmarkModel.getDefaultFolder(), item.getParentId());
                assertEquals(mTestPage, item.getUrl());
                assertEquals(TEST_PAGE_TITLE_GOOGLE, item.getTitle());
            }
        });
    }

    @SmallTest
    public void testOpenBookmark() throws InterruptedException, ExecutionException {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        openBookmarkManager();
        assertTrue("Grid view does not contain added bookmark: ",
                isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
        final View tile = getViewWithText(mItemsContainer, TEST_PAGE_TITLE_GOOGLE);
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), new Runnable() {
            @Override
            public void run() {
                TouchCommon.singleClickView(tile);
            }
        });
        assertEquals(TEST_PAGE_TITLE_GOOGLE, getActivity().getActivityTab().getTitle());
    }

    @SmallTest
    public void testUrlComposition() {
        BookmarkId mobileId = mBookmarkModel.getMobileFolderId();
        BookmarkId bookmarkBarId = mBookmarkModel.getDesktopFolderId();
        BookmarkId otherId = mBookmarkModel.getOtherFolderId();
        assertEquals("chrome-native://bookmarks/folder/" + mobileId,
                BookmarkUIState.createFolderUrl(mobileId).toString());
        assertEquals("chrome-native://bookmarks/folder/" + bookmarkBarId,
                BookmarkUIState.createFolderUrl(bookmarkBarId).toString());
        assertEquals("chrome-native://bookmarks/folder/" + otherId,
                BookmarkUIState.createFolderUrl(otherId).toString());
    }

    @SmallTest
    public void testOpenBookmarkManager() throws InterruptedException {
        openBookmarkManager();
        BookmarkDelegate delegate =
                ((BookmarkItemsAdapter) mItemsContainer.getAdapter()).getDelegateForTesting();
        assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());
        assertEquals("chrome-native://bookmarks/folder/3",
                BookmarkUtils.getLastUsedUrl(getActivity()));
    }

    @MediumTest
    public void testTopLevelFolders() throws InterruptedException {
        openBookmarkManager();
        final BookmarkDelegate delegate =
                ((BookmarkItemsAdapter) mItemsContainer.getAdapter()).getDelegateForTesting();
        final BookmarkActionBar toolbar = ((BookmarkManager) delegate).getToolbarForTests();

        // Open the "Mobile bookmarks" folder.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                delegate.openFolder(mBookmarkModel.getMobileFolderId());
            }
        });

        assertEquals(SelectableListToolbar.NAVIGATION_BUTTON_BACK,
                toolbar.getNavigationButtonForTests());
        assertFalse(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Call BookmarkActionBar#onClick() to activate the navigation button.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                toolbar.onClick(toolbar);
            }
        });

        // Check that we are in the root folder.
        assertEquals("Bookmarks", toolbar.getTitle());
        assertEquals(SelectableListToolbar.NAVIGATION_BUTTON_NONE,
                toolbar.getNavigationButtonForTests());
        assertFalse(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());
    }

    @MediumTest
    public void testSearchBookmarks() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(
                BookmarkPromoHeader.PromoState.PROMO_SIGNIN_OLD);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo);
        openBookmarkManager();

        BookmarkItemsAdapter adapter = ((BookmarkItemsAdapter) mItemsContainer.getAdapter());
        final BookmarkDelegate delegate = adapter.getDelegateForTesting();

        assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());
        assertBookmarkItems("Wrong number of items before starting search.", 3, adapter, delegate);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                delegate.openSearchUI();
            }
        });

        assertEquals(BookmarkUIState.STATE_SEARCHING, delegate.getCurrentState());
        assertBookmarkItems(
                "Wrong number of items after showing search UI. The promo should be hidden.", 2,
                adapter, delegate);

        searchBookmarks("Google");
        assertBookmarkItems("Wrong number of items after searching.", 1,
                mItemsContainer.getAdapter(), delegate);

        BookmarkId newBookmark = addBookmark(TEST_PAGE_TITLE_GOOGLE2, mTestPage);
        assertBookmarkItems("Wrong number of items after bookmark added while searching.", 2,
                mItemsContainer.getAdapter(), delegate);

        removeBookmark(newBookmark);
        assertBookmarkItems("Wrong number of items after bookmark removed while searching.", 1,
                mItemsContainer.getAdapter(), delegate);

        searchBookmarks("Non-existent page");
        assertBookmarkItems("Wrong number of items after searching for non-existent item.", 0,
                mItemsContainer.getAdapter(), delegate);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                delegate.closeSearchUI();
            }
        });
        assertBookmarkItems("Wrong number of items after closing search UI.", 3,
                mItemsContainer.getAdapter(), delegate);
        assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());
    }

    /**
     * Asserts the number of bookmark items being shown, taking large device deviders into account.
     *
     * @param errorMessage              Error message to display in case the assert fails.
     * @param exepectedOnRegularDevice  Expected count of items on small tablets.
     * @param adapter                   Adapter to retrieve the bookmark item count from.
     * @param delegate                  BookmarkDelegate to check the bookmark UI state.
     */
    private void assertBookmarkItems(final String errorMessage, final int exepectedOnRegularDevice,
            final Adapter adapter, final BookmarkDelegate delegate) {
        // TODO(twellington): Remove after bookmarks redesign is complete.
        // The +1 for large devices stems from the divider being added to the state folder for now,
        // which will offset all counts by one.
        final int expectedCount = DeviceFormFactor.isLargeTablet(getActivity())
                        && BookmarkUIState.STATE_FOLDER == delegate.getCurrentState()
                ? exepectedOnRegularDevice + 1
                : exepectedOnRegularDevice;
        assertEquals(errorMessage, expectedCount, adapter.getItemCount());
    }

    /**
     * Returns the View that has the given text.
     *
     * @param viewGroup    The group to which the view belongs.
     * @param expectedText The expected description text.
     * @return The unique view, if one exists. Throws an exception if one doesn't exist.
     */
    private static View getViewWithText(final ViewGroup viewGroup, final String expectedText) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<View>() {
            @Override
            public View call() throws Exception {
                ArrayList<View> outViews = new ArrayList<View>();
                ArrayList<View> matchingViews = new ArrayList<View>();
                viewGroup.findViewsWithText(outViews, expectedText, View.FIND_VIEWS_WITH_TEXT);
                // outViews includes all views whose text contains expectedText as a
                // case-insensitive substring. Filter these views to find only exact string matches.
                for (View v : outViews) {
                    if (TextUtils.equals(((TextView) v).getText().toString(), expectedText)) {
                        matchingViews.add(v);
                    }
                }
                Assert.assertEquals("Exactly one item should be present.", 1, matchingViews.size());
                return matchingViews.get(0);
            }
        });
    }

    private BookmarkId addBookmark(final String title, final String url) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<BookmarkId>() {
            @Override
            public BookmarkId call() throws Exception {
                return mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(), 0, title, url);
            }
        });
    }

    private void removeBookmark(final BookmarkId bookmarkId) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBookmarkModel.deleteBookmark(bookmarkId);
            }
        });
    }

    private void searchBookmarks(final String query) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((BookmarkItemsAdapter) mItemsContainer.getAdapter()).search(query);
            }
        });
    }
}
