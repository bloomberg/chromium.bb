// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.test.suitebuilder.annotation.LargeTest;
import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.BookmarksBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test suite for the bookmarks sync data type.
 */
public class BookmarksTest extends SyncTestBase {
    private static final String TAG = "BookmarksTest";

    private static final String BOOKMARKS_TYPE_STRING = "Bookmarks";

    private static final String URL = "http://chromium.org/";
    private static final String TITLE = "Chromium";
    private static final String MODIFIED_TITLE = "Chromium2";

    private BookmarksBridge mBookmarksBridge;

    private static class Bookmark {
        public final String id;
        public final String title;
        public final String url;

        private Bookmark(String id, String title, String url) {
            this.id = id;
            this.title = title;
            this.url = url;
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBookmarksBridge = new BookmarksBridge(Profile.getLastUsedProfile());
                mBookmarksBridge.loadEmptyPartnerBookmarkShimForTesting();
            }
        });
    }

    // Test syncing bookmark data from server to client: a new bookmark and a tombstone.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadAndDeleteBookmark() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        assertClientBookmarkCount(0);

        addServerBookmarkAndSync(TITLE, URL);
        List<Bookmark> bookmarks = getClientBookmarks();
        assertEquals("Only the injected bookmark should exist on the client.",
                1, bookmarks.size());
        Bookmark bookmark = bookmarks.get(0);
        assertEquals("The wrong title was found for the bookmark.", TITLE, bookmark.title);
        assertEquals("The wrong URL was found for the bookmark.", URL, bookmark.url);

        mFakeServerHelper.deleteEntity(bookmark.id);
        waitForServerBookmarkCountWithName(0, TITLE);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        waitForClientBookmarkCount(0);
    }

    // Test syncing bookmark data from client to server: a new bookmark, a modification,
    // and a tombstone.
    @LargeTest
    @Feature({"Sync"})
    public void testUploadModifyAndDeleteBookmark() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        assertClientBookmarkCount(0);
        assertServerBookmarkCountWithName(0, TITLE);
        assertServerBookmarkCountWithName(0, MODIFIED_TITLE);

        BookmarkId bookmarkId = addClientBookmark(TITLE, URL);
        waitForClientBookmarkCount(1);
        waitForServerBookmarkCountWithName(1, TITLE);

        setClientBookmarkTitle(bookmarkId, MODIFIED_TITLE);
        waitForServerBookmarkCountWithName(1, MODIFIED_TITLE);
        assertServerBookmarkCountWithName(0, TITLE);

        deleteClientBookmark(bookmarkId);
        waitForClientBookmarkCount(0);
        waitForServerBookmarkCountWithName(0, TITLE);
        assertServerBookmarkCountWithName(0, MODIFIED_TITLE);
    }

    private BookmarkId addClientBookmark(final String title, final String url) {
        final AtomicReference<BookmarkId> id = new AtomicReference<BookmarkId>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                BookmarkId parentId = mBookmarksBridge.getMobileFolderId();
                id.set(mBookmarksBridge.addBookmark(parentId, 0, title, url));
            }
        });
        assertNotNull("Failed to create bookmark.", id.get());
        return id.get();
    }

    private void addServerBookmarkAndSync(String title, String url) throws InterruptedException {
        mFakeServerHelper.injectBookmarkEntity(
                title, url, mFakeServerHelper.getBookmarkBarFolderId());
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
    }

    private void deleteClientBookmark(final BookmarkId id) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBookmarksBridge.deleteBookmark(id);
            }
        });
    }

    private void setClientBookmarkTitle(final BookmarkId id, final String title) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mBookmarksBridge.setBookmarkTitle(id, title);
            }
        });
    }

    private List<Bookmark> getClientBookmarks() throws JSONException {
        List<Pair<String, JSONObject>> rawBookmarks = SyncTestUtil.getLocalData(
                mContext, BOOKMARKS_TYPE_STRING);
        List<Bookmark> bookmarks = new ArrayList<Bookmark>(rawBookmarks.size());
        for (Pair<String, JSONObject> rawBookmark : rawBookmarks) {
            String id = rawBookmark.first;
            JSONObject json = rawBookmark.second;
            bookmarks.add(new Bookmark(id, json.getString("title"), json.getString("url")));
        }
        return bookmarks;
    }

    private void assertClientBookmarkCount(int count) throws JSONException {
        assertEquals("There should be " + count + " local bookmarks.",
                count, SyncTestUtil.getLocalData(mContext, BOOKMARKS_TYPE_STRING).size());
    }

    private void assertServerBookmarkCountWithName(int count, String name) {
        assertTrue("There should be " + count + " remote bookmarks with name " + name + ".",
                mFakeServerHelper.verifyEntityCountByTypeAndName(
                            count, ModelType.BOOKMARK, name));
    }

    private void waitForClientBookmarkCount(final int n) throws InterruptedException {
        boolean success = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return SyncTestUtil.getLocalData(mContext, BOOKMARKS_TYPE_STRING).size() == n;
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("There should be " + n + " local bookmarks.", success);
    }

    private void waitForServerBookmarkCountWithName(final int count, final String name)
            throws InterruptedException {
        boolean success = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return mFakeServerHelper.verifyEntityCountByTypeAndName(
                            count, ModelType.BOOKMARK, name);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Expected " + count + " remote bookmarks with name " + name + ".", success);
    }
}
