// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.test.suitebuilder.annotation.LargeTest;
import android.util.Pair;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.json.JSONObject;

import java.util.List;

/**
 * Test suite for the bookmarks sync data type.
 */
public class BookmarksTest extends SyncTestBase {
    private static final String TAG = "BookmarksTest";

    private static final String BOOKMARKS_TYPE_STRING = "Bookmarks";

    /**
     * This method performs all steps to test that the client successfully downloads a bookmark from
     * the server. If successful, a single bookmark will exist on the fake Sync server and on the
     * client. This state can then be used to test deletion or modification of the bookmark.
     *
     * @return the downloaded bookmark's server ID
     */
    private String runDownloadBookmarkTestScenario() throws Exception {
        setupTestAccountAndSignInToSync(CLIENT_ID);
        assertEquals("No bookmarks should exist on the client by default.",
                0, SyncTestUtil.getLocalData(mContext, BOOKMARKS_TYPE_STRING).size());

        String title = "Title";
        String url = "http://chromium.org/";
        mFakeServerHelper.injectBookmarkEntity(
                title, url, mFakeServerHelper.getBookmarkBarFolderId());

        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);

        List<Pair<String, JSONObject>> bookmarks = SyncTestUtil.getLocalData(
                mContext, BOOKMARKS_TYPE_STRING);
        assertEquals("Only the injected bookmark should exist on the client.",
                1, bookmarks.size());
        Pair<String, JSONObject> bookmark = bookmarks.get(0);
        JSONObject bookmarkJson = bookmark.second;
        assertEquals("The wrong title was found for the bookmark.", title,
                bookmarkJson.getString("title"));
        assertEquals("The wrong URL was found for the bookmark.",
                url, bookmarkJson.getString("url"));

        return bookmark.first;
    }

    @LargeTest
    @Feature({"Sync"})
    public void testDownloadBookmark() throws Exception {
        runDownloadBookmarkTestScenario();
    }

    @LargeTest
    @Feature({"Sync"})
    public void testDownloadDeletedBookmark() throws Exception {
        String id = runDownloadBookmarkTestScenario();
        mFakeServerHelper.deleteEntity(id);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);

        boolean bookmarkDeleted = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return SyncTestUtil.getLocalData(mContext, BOOKMARKS_TYPE_STRING).isEmpty();
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);

        assertTrue("The lone bookmark should have been deleted.", bookmarkDeleted);
    }
}
