// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

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
import java.util.HashMap;
import java.util.List;

/**
 * Tests for bookmark bridge
 */
public class BookmarksBridgeTest extends ChromeShellTestBase {

    private ChromeShellActivity mActivity;
    private Profile mProfile;
    private BookmarksBridge mBookmarksBridge;
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
                mProfile = tab.getProfile();
                mBookmarksBridge = new BookmarksBridge(mProfile);
                mBookmarksBridge.loadEmptyPartnerBookmarkShimForTesting();
                mMobileNode = mBookmarksBridge.getMobileFolderId();
                mDesktopNode = mBookmarksBridge.getDesktopFolderId();
                mOtherNode = mBookmarksBridge.getOtherFolderId();
            }
        });
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark"})
    public void testAddBookmarksAndFolders() {
        BookmarkId bookmarkA = mBookmarksBridge.addBookmark(mDesktopNode, 0, "a", "http://a.com");
        verifyBookmark(bookmarkA, "a", "http://a.com/", false, mDesktopNode);
        BookmarkId bookmarkB = mBookmarksBridge.addBookmark(mOtherNode, 0, "b", "http://b.com");
        verifyBookmark(bookmarkB, "b", "http://b.com/", false, mOtherNode);
        BookmarkId bookmarkC = mBookmarksBridge.addBookmark(mMobileNode, 0, "c", "http://c.com");
        verifyBookmark(bookmarkC, "c", "http://c.com/", false, mMobileNode);
        BookmarkId folderA = mBookmarksBridge.addFolder(mOtherNode, 0, "fa");
        verifyBookmark(folderA, "fa", null, true, mOtherNode);
        BookmarkId folderB = mBookmarksBridge.addFolder(mDesktopNode, 0, "fb");
        verifyBookmark(folderB, "fb", null, true, mDesktopNode);
        BookmarkId folderC = mBookmarksBridge.addFolder(mMobileNode, 0, "fc");
        verifyBookmark(folderC, "fc", null, true, mMobileNode);
        BookmarkId bookmarkAA = mBookmarksBridge.addBookmark(folderA, 0, "aa", "http://aa.com");
        verifyBookmark(bookmarkAA, "aa", "http://aa.com/", false, folderA);
        BookmarkId folderAA = mBookmarksBridge.addFolder(folderA, 0, "faa");
        verifyBookmark(folderAA, "faa", null, true, folderA);
    }

    private void verifyBookmark(BookmarkId idToVerify, String expectedTitle,
            String expectedUrl, boolean isFolder, BookmarkId expectedParent) {
        assertNotNull(idToVerify);
        BookmarkItem item = mBookmarksBridge.getBookmarkById(idToVerify);
        assertEquals(expectedTitle, item.getTitle());
        assertEquals(item.isFolder(), isFolder);
        if (!isFolder) assertEquals(expectedUrl, item.getUrl());
        assertEquals(item.getParentId(), expectedParent);
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark"})
    public void testGetAllFoldersWithDepths() {
        BookmarkId folderA = mBookmarksBridge.addFolder(mMobileNode, 0, "a");
        BookmarkId folderB = mBookmarksBridge.addFolder(mDesktopNode, 0, "b");
        BookmarkId folderC = mBookmarksBridge.addFolder(mOtherNode, 0, "c");
        BookmarkId folderAA = mBookmarksBridge.addFolder(folderA, 0, "aa");
        BookmarkId folderBA = mBookmarksBridge.addFolder(folderB, 0, "ba");
        BookmarkId folderAAA = mBookmarksBridge.addFolder(folderAA, 0, "aaa");
        BookmarkId folderAAAA = mBookmarksBridge.addFolder(folderAAA, 0, "aaaa");

        mBookmarksBridge.addBookmark(mMobileNode, 0, "ua", "http://www.google.com");
        mBookmarksBridge.addBookmark(mDesktopNode, 0, "ua", "http://www.google.com");
        mBookmarksBridge.addBookmark(mOtherNode, 0, "ua", "http://www.google.com");
        mBookmarksBridge.addBookmark(folderA, 0, "ua", "http://www.medium.com");

        // Map folders to depths as expected results
        HashMap<BookmarkId, Integer> idToDepth = new HashMap<BookmarkId, Integer>();
        idToDepth.put(folderA, 0);
        idToDepth.put(folderAA, 1);
        idToDepth.put(folderAAA, 2);
        idToDepth.put(folderAAAA, 3);
        idToDepth.put(mDesktopNode, 0);
        idToDepth.put(folderB, 1);
        idToDepth.put(folderBA, 2);
        idToDepth.put(folderC, 0);

        List<BookmarkId> folderList = new ArrayList<BookmarkId>();
        List<Integer> depthList = new ArrayList<Integer>();
        mBookmarksBridge.getAllFoldersWithDepths(folderList, depthList);

        assertEquals("Returned folder list has different size (" + folderList.size()
                + ") from depth list (" + depthList.size() + ").",
                folderList.size(), depthList.size());
        assertEquals("Returned lists have different sizes (" + folderList.size()
                + ") from expected size (" + idToDepth.size() + ").",
                folderList.size(), idToDepth.size());

        for (int i = 0; i < folderList.size(); i++) {
            assertNotNull(folderList.get(i));
            assertNotNull(depthList.get(i));
            assertTrue("Returned list contained unexpected key: " + folderList.get(i),
                    idToDepth.containsKey(folderList.get(i)));
            assertEquals(depthList.get(i), idToDepth.get(folderList.get(i)));
            idToDepth.remove(folderList.get(i));
        }

        assertEquals("Expected list contains elements that are not in returned list",
                idToDepth.size(), 0);
    }
}
