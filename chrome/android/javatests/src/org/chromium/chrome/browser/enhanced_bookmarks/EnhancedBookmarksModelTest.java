// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhanced_bookmarks;

import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellTab;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Stack;

/**
 * Tests for {@link EnhancedBookmarksModel}, the data layer of Enhanced Bookmarks.
 */
public class EnhancedBookmarksModelTest extends ChromeShellTestBase{

    private ChromeShellActivity mActivity;
    private EnhancedBookmarksModel mBookmarksModel;
    private BookmarkId mMobileNode;
    private BookmarkId mOtherNode;
    private BookmarkId mDesktopNode;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchChromeShellWithBlankPage();
        assertTrue(waitForActiveShellToBeDoneLoading());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeShellTab tab = mActivity.getActiveTab();
                Profile profile = tab.getProfile();
                mBookmarksModel = new EnhancedBookmarksModel(profile);
                mBookmarksModel.loadEmptyPartnerBookmarkShimForTesting();
                mMobileNode = mBookmarksModel.getMobileFolderId();
                mDesktopNode = mBookmarksModel.getDesktopFolderId();
                mOtherNode = mBookmarksModel.getOtherFolderId();
            }
        });
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark"})
    public void testGetAllBookmarkIDsOrderedByCreationDate() throws InterruptedException {
        BookmarkId folderA = mBookmarksModel.addFolder(mMobileNode, 0, "a");
        BookmarkId folderB = mBookmarksModel.addFolder(mDesktopNode, 0, "b");

        Stack<BookmarkId> stack = new Stack<BookmarkId>();
        stack.push(mBookmarksModel.addBookmark(folderA, 0, "a", "http://www.medium.com"));
        // If add bookmarks too fast, eventually some bookmarks will have the same timestamp, which
        // confuses the bookmark model.
        Thread.sleep(20);
        stack.push(mBookmarksModel.addBookmark(folderB, 0, "b", "http://aurimas.com"));
        Thread.sleep(20);
        stack.push(mBookmarksModel.addBookmark(mMobileNode, 0, "c", "http://www.aurimas.com"));
        Thread.sleep(20);
        stack.push(mBookmarksModel.addBookmark(mDesktopNode, 0, "d", "http://www.aurimas.org"));
        Thread.sleep(20);
        stack.push(mBookmarksModel.addBookmark(mOtherNode, 0, "e", "http://www.google.com"));
        Thread.sleep(20);
        stack.push(mBookmarksModel.addBookmark(folderA, 0, "f", "http://www.newt.com"));
        Thread.sleep(20);
        stack.push(mBookmarksModel.addBookmark(folderB, 0, "g", "http://kkimlabs.com"));

        List<BookmarkId> bookmarks = mBookmarksModel.getAllBookmarkIDsOrderedByCreationDate();
        assertEquals(bookmarks.size(), stack.size());
        for (BookmarkId returnedBookmark : bookmarks) {
            assertEquals(stack.pop(), returnedBookmark);
        }
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark"})
    public void testBookmarkPropertySetters() {
        BookmarkId folderA = mBookmarksModel.addFolder(mMobileNode, 0, "a");

        BookmarkId bookmarkA = mBookmarksModel.addBookmark(mDesktopNode, 0, "a", "http://a.com");
        BookmarkId bookmarkB = mBookmarksModel.addBookmark(mMobileNode, 0, "a", "http://a.com");
        BookmarkId bookmarkC = mBookmarksModel.addBookmark(mOtherNode, 0, "a", "http://a.com");
        BookmarkId bookmarkD = mBookmarksModel.addBookmark(folderA, 0, "a", "http://a.com");

        mBookmarksModel.setBookmarkTitle(folderA, "hauri");
        assertEquals("hauri", mBookmarksModel.getBookmarkTitle(folderA));

        mBookmarksModel.setBookmarkTitle(bookmarkA, "auri");
        mBookmarksModel.setBookmarkUrl(bookmarkA, "http://auri.org/");
        mBookmarksModel.setBookmarkDescription(bookmarkA, "auri");
        verifyBookmark(bookmarkA, "auri", "http://auri.org/", false, mDesktopNode, "auri");

        mBookmarksModel.setBookmarkTitle(bookmarkB, "lauri");
        mBookmarksModel.setBookmarkUrl(bookmarkB, "http://lauri.org/");
        mBookmarksModel.setBookmarkDescription(bookmarkB, "lauri");
        verifyBookmark(bookmarkB, "lauri", "http://lauri.org/", false, mMobileNode, "lauri");

        mBookmarksModel.setBookmarkTitle(bookmarkC, "mauri");
        mBookmarksModel.setBookmarkUrl(bookmarkC, "http://mauri.org/");
        mBookmarksModel.setBookmarkDescription(bookmarkC, "mauri");
        verifyBookmark(bookmarkC, "mauri", "http://mauri.org/", false, mOtherNode, "mauri");

        mBookmarksModel.setBookmarkTitle(bookmarkD, "kauri");
        mBookmarksModel.setBookmarkUrl(bookmarkD, "http://kauri.org/");
        mBookmarksModel.setBookmarkDescription(bookmarkD, "kauri");
        verifyBookmark(bookmarkD, "kauri", "http://kauri.org/", false, folderA, "kauri");
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark"})
    public void testMoveBookmarks() {
        BookmarkId bookmarkA = mBookmarksModel.addBookmark(mDesktopNode, 0, "a", "http://a.com");
        BookmarkId bookmarkB = mBookmarksModel.addBookmark(mOtherNode, 0, "b", "http://b.com");
        BookmarkId bookmarkC = mBookmarksModel.addBookmark(mMobileNode, 0, "c", "http://c.com");
        BookmarkId folderA = mBookmarksModel.addFolder(mOtherNode, 0, "fa");
        BookmarkId folderB = mBookmarksModel.addFolder(mDesktopNode, 0, "fb");
        BookmarkId folderC = mBookmarksModel.addFolder(mMobileNode, 0, "fc");
        BookmarkId bookmarkAA = mBookmarksModel.addBookmark(folderA, 0, "aa", "http://aa.com");
        BookmarkId bookmarkCA = mBookmarksModel.addBookmark(folderC, 0, "ca", "http://ca.com");
        BookmarkId folderAA = mBookmarksModel.addFolder(folderA, 0, "faa");

        HashSet<BookmarkId> movedBookmarks = new HashSet<BookmarkId>(6);
        movedBookmarks.add(bookmarkA);
        movedBookmarks.add(bookmarkB);
        movedBookmarks.add(bookmarkC);
        movedBookmarks.add(folderC);
        movedBookmarks.add(folderB);
        movedBookmarks.add(bookmarkAA);
        mBookmarksModel.moveBookmarks(new ArrayList<BookmarkId>(movedBookmarks), folderAA);

        // Order of the moved bookmarks is not tested.
        verifyBookmarkListNoOrder(mBookmarksModel.getChildIDs(folderAA, true, true),
                movedBookmarks);
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark"})
    public void testGetChildIDs() {
        BookmarkId folderA = mBookmarksModel.addFolder(mMobileNode, 0, "fa");
        HashSet<BookmarkId> expectedChildren = new HashSet<>();
        expectedChildren.add(mBookmarksModel.addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(mBookmarksModel.addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(mBookmarksModel.addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(mBookmarksModel.addBookmark(folderA, 0, "a", "http://a.com"));
        BookmarkId folderAA = mBookmarksModel.addFolder(folderA, 0, "faa");
        // urls only
        verifyBookmarkListNoOrder(mBookmarksModel.getChildIDs(folderA, false, true),
                expectedChildren);
        // folders only
        verifyBookmarkListNoOrder(mBookmarksModel.getChildIDs(folderA, true, false),
                new HashSet<BookmarkId>(Arrays.asList(folderAA)));
        // folders and urls
        expectedChildren.add(folderAA);
        verifyBookmarkListNoOrder(mBookmarksModel.getChildIDs(folderA, true, true),
                expectedChildren);
    }

    // Moved from BookmarksBridgeTest
    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark"})
    public void testAddBookmarksAndFolders() {
        BookmarkId bookmarkA = mBookmarksModel.addBookmark(mDesktopNode, 0, "a", "http://a.com");
        verifyBookmark(bookmarkA, "a", "http://a.com/", false, mDesktopNode, "");

        BookmarkId bookmarkB = mBookmarksModel.addBookmark(mOtherNode, 0, "b", "http://b.com");
        verifyBookmark(bookmarkB, "b", "http://b.com/", false, mOtherNode, "");

        BookmarkId bookmarkC = mBookmarksModel.addBookmark(mMobileNode, 0, "c", "http://c.com");
        verifyBookmark(bookmarkC, "c", "http://c.com/", false, mMobileNode, "");

        BookmarkId folderA = mBookmarksModel.addFolder(mOtherNode, 0, "fa");
        verifyBookmark(folderA, "fa", null, true, mOtherNode, "");

        BookmarkId folderB = mBookmarksModel.addFolder(mDesktopNode, 0, "fb");
        verifyBookmark(folderB, "fb", null, true, mDesktopNode, "");

        BookmarkId folderC = mBookmarksModel.addFolder(mMobileNode, 0, "fc");
        verifyBookmark(folderC, "fc", null, true, mMobileNode, "");

        BookmarkId bookmarkAA = mBookmarksModel.addBookmark(folderA, 0, "aa", "http://aa.com");
        verifyBookmark(bookmarkAA, "aa", "http://aa.com/", false, folderA, "");

        BookmarkId folderAA = mBookmarksModel.addFolder(folderA, 0, "faa");
        verifyBookmark(folderAA, "faa", null, true, folderA, "");
    }

    private void verifyBookmark(BookmarkId idToVerify, String expectedTitle,
            String expectedUrl, boolean isFolder, BookmarkId expectedParent,
            String expectedDescription) {
        assertNotNull(idToVerify);
        BookmarkItem item = mBookmarksModel.getBookmarkById(idToVerify);
        assertEquals(expectedTitle, item.getTitle());
        assertEquals(isFolder, item.isFolder());
        if (!isFolder) assertEquals(expectedUrl, item.getUrl());
        assertEquals(expectedParent, item.getParentId());
        assertEquals(expectedDescription, mBookmarksModel.getBookmarkDescription(idToVerify));
    }

    /**
     * Before using this helper method, always make sure @param listToVerify does not contain
     * duplicates.
     */
    private void verifyBookmarkListNoOrder(List<BookmarkId> listToVerify,
            HashSet<BookmarkId> expectedIds) {
        HashSet<BookmarkId> expectedIdsCopy = new HashSet<>(expectedIds);
        assertEquals(expectedIdsCopy.size(), listToVerify.size());
        for (BookmarkId id : listToVerify) {
            assertNotNull(id);
            assertTrue("List contains wrong element: ", expectedIdsCopy.contains(id));
            expectedIdsCopy.remove(id);
        }
        assertTrue("List does not contain some expected bookmarks: ", expectedIdsCopy.isEmpty());
    }
}
