// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.bookmark.BookmarksBridge.BookmarkItem;
import org.chromium.chrome.browser.enhancedbookmarks.EnhancedBookmarksModel.AddBookmarkCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Stack;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for {@link EnhancedBookmarksModel}, the data layer of Enhanced Bookmarks.
 */
public class EnhancedBookmarksModelTest extends NativeLibraryTestBase {
    private static final int TIMEOUT_MS = 5000;
    private EnhancedBookmarksModel mBookmarksModel;
    private BookmarkId mMobileNode;
    private BookmarkId mOtherNode;
    private BookmarkId mDesktopNode;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Profile profile = Profile.getLastUsedProfile();
                mBookmarksModel = new EnhancedBookmarksModel(profile);
                mBookmarksModel.loadEmptyPartnerBookmarkShimForTesting();
            }
        });

        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mBookmarksModel.isBookmarkModelLoaded();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
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
        stack.push(addBookmark(folderA, 0, "a", "http://www.medium.com"));
        // If add bookmarks too fast, eventually some bookmarks will have the same timestamp, which
        // confuses the bookmark model.
        Thread.sleep(20);
        stack.push(addBookmark(folderB, 0, "b", "http://aurimas.com"));
        Thread.sleep(20);
        stack.push(addBookmark(mMobileNode, 0, "c", "http://www.aurimas.com"));
        Thread.sleep(20);
        stack.push(addBookmark(mDesktopNode, 0, "d", "http://www.aurimas.org"));
        Thread.sleep(20);
        stack.push(addBookmark(mOtherNode, 0, "e", "http://www.google.com"));
        Thread.sleep(20);
        stack.push(addBookmark(folderA, 0, "f", "http://www.newt.com"));
        Thread.sleep(20);
        stack.push(addBookmark(folderB, 0, "g", "http://kkimlabs.com"));

        List<BookmarkId> bookmarks = mBookmarksModel.getAllBookmarkIDsOrderedByCreationDate();
        assertEquals(stack.size(), bookmarks.size());
        for (BookmarkId returnedBookmark : bookmarks) {
            assertEquals(stack.pop(), returnedBookmark);
        }
    }

    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark"})
    public void testBookmarkPropertySetters() {
        BookmarkId folderA = mBookmarksModel.addFolder(mMobileNode, 0, "a");

        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", "http://a.com");
        BookmarkId bookmarkB = addBookmark(mMobileNode, 0, "a", "http://a.com");
        BookmarkId bookmarkC = addBookmark(mOtherNode, 0, "a", "http://a.com");
        BookmarkId bookmarkD = addBookmark(folderA, 0, "a", "http://a.com");

        mBookmarksModel.setBookmarkTitle(folderA, "hauri");
        assertEquals("hauri", mBookmarksModel.getBookmarkTitle(folderA));

        mBookmarksModel.setBookmarkTitle(bookmarkA, "auri");
        mBookmarksModel.setBookmarkUrl(bookmarkA, "http://auri.org/");
        verifyBookmark(bookmarkA, "auri", "http://auri.org/", false, mDesktopNode);

        mBookmarksModel.setBookmarkTitle(bookmarkB, "lauri");
        mBookmarksModel.setBookmarkUrl(bookmarkB, "http://lauri.org/");
        verifyBookmark(bookmarkB, "lauri", "http://lauri.org/", false, mMobileNode);

        mBookmarksModel.setBookmarkTitle(bookmarkC, "mauri");
        mBookmarksModel.setBookmarkUrl(bookmarkC, "http://mauri.org/");
        verifyBookmark(bookmarkC, "mauri", "http://mauri.org/", false, mOtherNode);

        mBookmarksModel.setBookmarkTitle(bookmarkD, "kauri");
        mBookmarksModel.setBookmarkUrl(bookmarkD, "http://kauri.org/");
        verifyBookmark(bookmarkD, "kauri", "http://kauri.org/", false, folderA);
    }


    @UiThreadTest
    @SmallTest
    @Feature({"Bookmark" })
    @SuppressFBWarnings("DLS_DEAD_LOCAL_STORE")
    public void testMoveBookmarks() {
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", "http://a.com");
        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", "http://b.com");
        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", "http://c.com");
        BookmarkId folderA = mBookmarksModel.addFolder(mOtherNode, 0, "fa");
        BookmarkId folderB = mBookmarksModel.addFolder(mDesktopNode, 0, "fb");
        BookmarkId folderC = mBookmarksModel.addFolder(mMobileNode, 0, "fc");
        BookmarkId bookmarkAA = addBookmark(folderA, 0, "aa", "http://aa.com");
        BookmarkId bookmarkCA = addBookmark(folderC, 0, "ca", "http://ca.com");
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
        expectedChildren.add(addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(addBookmark(folderA, 0, "a", "http://a.com"));
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
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", "http://a.com");
        verifyBookmark(bookmarkA, "a", "http://a.com/", false, mDesktopNode);

        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", "http://b.com");
        verifyBookmark(bookmarkB, "b", "http://b.com/", false, mOtherNode);

        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", "http://c.com");
        verifyBookmark(bookmarkC, "c", "http://c.com/", false, mMobileNode);

        BookmarkId folderA = mBookmarksModel.addFolder(mOtherNode, 0, "fa");
        verifyBookmark(folderA, "fa", null, true, mOtherNode);

        BookmarkId folderB = mBookmarksModel.addFolder(mDesktopNode, 0, "fb");
        verifyBookmark(folderB, "fb", null, true, mDesktopNode);

        BookmarkId folderC = mBookmarksModel.addFolder(mMobileNode, 0, "fc");
        verifyBookmark(folderC, "fc", null, true, mMobileNode);

        BookmarkId bookmarkAA = addBookmark(folderA, 0, "aa", "http://aa.com");
        verifyBookmark(bookmarkAA, "aa", "http://aa.com/", false, folderA);

        BookmarkId folderAA = mBookmarksModel.addFolder(folderA, 0, "faa");
        verifyBookmark(folderAA, "faa", null, true, folderA);
    }

    @UiThreadTest
    @SmallTest
    @CommandLineFlags.Add({ChromeSwitches.ENABLE_OFFLINE_PAGES})
    @Feature({"Bookmark"})
    public void testOfflineBridgeLoaded() {
        assertTrue(mBookmarksModel.getOfflinePageBridge() != null);
        assertTrue(mBookmarksModel.getOfflinePageBridge().isOfflinePageModelLoaded());
    }

    private BookmarkId addBookmark(final BookmarkId parent, final int index, final String title,
            final String url) {
        final AtomicReference<BookmarkId> result = new AtomicReference<BookmarkId>();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBookmarksModel.addBookmarkAsync(
                        parent, index, title, url, null, new AddBookmarkCallback() {
                            @Override
                            public void onBookmarkAdded(
                                    final BookmarkId bookmarkId, int saveResult) {
                                result.set(bookmarkId);
                                semaphore.release();
                            }
                        });
            }
        });
        try {
            if (semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                return result.get();
            } else {
                return null;
            }
        } catch (InterruptedException e) {
            return null;
        }
    }

    private void verifyBookmark(BookmarkId idToVerify, String expectedTitle,
            String expectedUrl, boolean isFolder, BookmarkId expectedParent) {
        assertNotNull(idToVerify);
        BookmarkItem item = mBookmarksModel.getBookmarkById(idToVerify);
        assertEquals(expectedTitle, item.getTitle());
        assertEquals(isFolder, item.isFolder());
        if (!isFolder) assertEquals(expectedUrl, item.getUrl());
        assertEquals(expectedParent, item.getParentId());
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
