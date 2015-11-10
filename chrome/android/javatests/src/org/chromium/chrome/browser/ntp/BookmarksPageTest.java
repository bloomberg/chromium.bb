// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.test.suitebuilder.annotation.LargeTest;
import android.text.TextUtils;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.bookmark.AddEditBookmarkFragment;
import org.chromium.chrome.browser.bookmark.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmark.ManageBookmarkActivity;
import org.chromium.chrome.browser.bookmark.SelectBookmarkFolderFragment;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.BookmarkTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content.browser.test.util.UiUtils;

import java.util.concurrent.Callable;

/**
 * Tests for the old bookmarks page.
 */
@CommandLineFlags.Add(ChromeSwitches.ENABLE_ENHANCED_BOOKMARKS + "=0")
public class BookmarksPageTest extends ChromeTabbedActivityTestBase {

    private static final String TEST_PAGE =
            TestHttpServerClient.getUrl("chrome/test/data/android/about.html");
    private static final String TEST_PAGE_TITLE = "About";
    private static final String TEST_FOLDER_TITLE = "Test Folder";
    private static final String TEST_PAGE_TITLE_2 = "About 2";
    private static final String MOBILE_BOOKMARKS_TITLE = "Mobile bookmarks";
    private static final String BOOKMARKS_TITLE = "Bookmarks";

    private ListView mBookmarksList;
    private LinearLayout mHierarchyLayout;

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void addBookmark() throws InterruptedException {
        loadUrl(TEST_PAGE);
        BookmarkTestUtils.addCurrentUrlAsBookmark(this, getActivity());
        loadMobileBookmarksPage();
    }

    private void addFolderAndAddBookmark() throws InterruptedException {
        loadUrl(TEST_PAGE);
        ManageBookmarkActivity addActivity = BookmarkTestUtils.selectBookmarkItemFromMenu(
                getInstrumentation(), getActivity());
        final AddEditBookmarkFragment addFragment =
                BookmarkTestUtils.loadAddEditFragment(addActivity);

        BookmarkTestUtils.clickSelectFolderButton(this, addFragment);
        SelectBookmarkFolderFragment selectedFolder = BookmarkTestUtils.loadSelectFragment(
                addActivity);

        BookmarkTestUtils.clickNewFolderButton(this, selectedFolder);
        final AddEditBookmarkFragment addNewFolderFragment =
                BookmarkTestUtils.loadAddFolderFragment(addActivity);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ((EditText) addNewFolderFragment.getView().findViewById(R.id.bookmark_title_input))
                        .setText(TEST_FOLDER_TITLE);
            }
        });
        BookmarkTestUtils.clickOkButton(this, addNewFolderFragment);
        BookmarkTestUtils.clickOkButton(this, addFragment);
        loadMobileBookmarksPage();
    }

    private void loadMobileBookmarksPage() throws InterruptedException {
        final String mobileFolderUrl = UrlConstants.BOOKMARKS_FOLDER_URL + "2";
        loadUrl(mobileFolderUrl);
        Tab tab = getActivity().getActivityTab();
        assertTrue(tab.getNativePage() instanceof BookmarksPage);
        mHierarchyLayout = (LinearLayout) getActivity().findViewById(
                R.id.bookmark_folder_structure);
        mBookmarksList = (ListView) getActivity().findViewById(R.id.bookmarks_list_view);
    }

    private void openBookmarkInCurrentTab(final BookmarkItemView itemView)
            throws InterruptedException {
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), new Runnable() {
            @Override
            public void run() {
                TouchCommon.singleClickView(itemView);
            }
        });
        BookmarkTestUtils.assertUrlBarEquals(
                getActivity(), "urlBar string not matching the bookmarked page", TEST_PAGE);
    }

    private void addBookmarkAndLongClickForContextMenu() throws InterruptedException {
        addBookmark();
        BookmarkItemView itemView = (BookmarkItemView) BookmarkTestUtils.getViewWithText(
                mBookmarksList, TEST_PAGE_TITLE);
        TouchCommon.longPressView(itemView);
    }

    private String getCurrentFolderTitle() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return ((BookmarkFolderHierarchyItem) mHierarchyLayout.getChildAt(
                        mHierarchyLayout.getChildCount() - 1)).getText().toString();
            }
        });
    }

    private void clickFolderInFolderHierarchy(final String folderToSelect)
            throws InterruptedException {
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return (BookmarkTestUtils.getViewWithText(mHierarchyLayout,
                        folderToSelect) != null);
            }
        });
        final BookmarkFolderHierarchyItem itemView =
                (BookmarkFolderHierarchyItem) BookmarkTestUtils.getViewWithText(
                        mHierarchyLayout, folderToSelect);
        TouchCommon.singleClickView(itemView);
        assertEquals(folderToSelect, getCurrentFolderTitle());
    }

    private void clickFolderInBookmarksList(final String folderToSelect)
            throws InterruptedException {
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return (BookmarkTestUtils.getViewWithText(mBookmarksList, folderToSelect) != null);
            }
        });
        final BookmarkItemView itemView = (BookmarkItemView) BookmarkTestUtils.getViewWithText(
                mBookmarksList, folderToSelect);
        TouchCommon.singleClickView(itemView);
        assertEquals(folderToSelect, getCurrentFolderTitle());
    }

    private boolean isItemPresentInBookmarksList(final String expectedTitle) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                for (int i = 0; i < mBookmarksList.getCount(); i++) {
                    String actualTitle =
                            ((BookmarkItem) mBookmarksList.getItemAtPosition(i)).getTitle();
                    if (TextUtils.equals(actualTitle, expectedTitle)) {
                        return true;
                    }
                }
                return false;
            }
        });
    }

    @LargeTest
    public void testCreateAndOpenBookmark() throws InterruptedException {
        addBookmark();
        // Assert "About" item is listed in the bookmarks list.
        assertTrue(isItemPresentInBookmarksList(TEST_PAGE_TITLE));
        // Click the item "About".
        openBookmarkInCurrentTab((BookmarkItemView) BookmarkTestUtils.getViewWithText(
                mBookmarksList, TEST_PAGE_TITLE));
    }

    @DisabledTest // Fails on android-one: crbug.com/540728
    @LargeTest
    public void testNavigateFoldersInFolderHierarchy() throws InterruptedException {
        addFolderAndAddBookmark();
        // Click on "Mobile bookmarks" in the Folder hierarchy.
        clickFolderInFolderHierarchy(MOBILE_BOOKMARKS_TITLE);
        // Assert "Test Folder" is listed in the bookmarks list.
        assertTrue(isItemPresentInBookmarksList(TEST_FOLDER_TITLE));
        // Click on "Bookmarks" in the Folder hierarchy.
        clickFolderInFolderHierarchy(BOOKMARKS_TITLE);
        // Assert "Desktop Bookmarks" is listed in the bookmarks list.
        assertTrue(isItemPresentInBookmarksList(MOBILE_BOOKMARKS_TITLE));
    }

    /*
    @LargeTest

    Disabled because of repeated flakes on ICS bot.
    http://crbug.com/384126
    */
    @DisabledTest
    public void testNavigateFoldersInBookmarksListView() throws InterruptedException {
        addFolderAndAddBookmark();
        // Click on "Bookmarks" in the Folder hierarchy.
        clickFolderInFolderHierarchy(BOOKMARKS_TITLE);
        // Assert "Mobile Bookmarks" is listed in the bookmarks list.
        assertTrue(isItemPresentInBookmarksList(MOBILE_BOOKMARKS_TITLE));
        // Click on "Mobile bookmarks" in the bookmarks list view.
        clickFolderInBookmarksList(MOBILE_BOOKMARKS_TITLE);
        // Assert "Test Folder" is listed in the bookmarks list.
        assertTrue(isItemPresentInBookmarksList(TEST_FOLDER_TITLE));
        // Click on "Test Folder" in the bookmarks list view.
        clickFolderInBookmarksList(TEST_FOLDER_TITLE);
        // Assert "About" is listed in the bookmarks list.
        assertTrue(isItemPresentInBookmarksList(TEST_PAGE_TITLE));
    }

    @LargeTest
    public void testContextMenuOptionOpenInANewTab() throws InterruptedException {
        addBookmark();
        BookmarkItemView itemView = (BookmarkItemView) BookmarkTestUtils.getViewWithText(
                mBookmarksList, TEST_PAGE_TITLE);
        invokeContextMenuAndOpenInANewTab(itemView, BookmarkItemView.ID_OPEN_IN_NEW_TAB, false,
                TEST_PAGE);
    }

    @LargeTest
    public void testContextMenuOptionOpenInAnIncognitoTab() throws InterruptedException {
        addBookmark();
        BookmarkItemView itemView = (BookmarkItemView) BookmarkTestUtils.getViewWithText(
                mBookmarksList, TEST_PAGE_TITLE);
        invokeContextMenuAndOpenInANewTab(itemView, BookmarkItemView.ID_OPEN_IN_INCOGNITO_TAB, true,
                TEST_PAGE);
    }

    @LargeTest
    public void testContextMenuOptionEditBookmark() throws InterruptedException {
        addBookmarkAndLongClickForContextMenu();
        // Invoke the "Edit Bookmark" context menu option.
        final ManageBookmarkActivity activity = ActivityUtils.waitForActivity(
                getInstrumentation(), ManageBookmarkActivity.class,
                new Runnable() {
                    @Override
                    public void run() {
                        getInstrumentation().invokeContextMenuAction(
                                getActivity(), BookmarkItemView.ID_EDIT, 0);
                    }
                }
        );
        UiUtils.settleDownUI(getInstrumentation());
        // Edit the bookmark title.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ((EditText) activity.findViewById(R.id.bookmark_title_input))
                        .setText(TEST_PAGE_TITLE_2);
            }
        });
        TestTouchUtils.clickView(this, activity.findViewById(R.id.ok));
        // Assert "About 2" is listed in the bookmarks list.
        assertTrue(isItemPresentInBookmarksList(TEST_PAGE_TITLE_2));
    }

    @LargeTest
    public void testContextMenuOptionDeleteBookmark() throws InterruptedException {
        addBookmarkAndLongClickForContextMenu();
        // Invoke the "Delete Bookmark" context menu option.
        getInstrumentation().invokeContextMenuAction(
                getActivity(), BookmarkItemView.ID_DELETE, 0);
        UiUtils.settleDownUI(getInstrumentation());
        // Assert no bookmarks exist in the current folder.
        assertTrue(mBookmarksList.getCount() == 0);
    }
}
