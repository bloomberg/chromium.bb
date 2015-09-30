// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.test.suitebuilder.annotation.LargeTest;
import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.sync.ModelType;
import org.chromium.sync.protocol.EntitySpecifics;
import org.chromium.sync.protocol.SyncEnums;
import org.chromium.sync.protocol.TypedUrlSpecifics;
import org.chromium.ui.base.PageTransition;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Test suite for the typed URLs sync data type.
 */
public class TypedUrlsTest extends SyncTestBase {
    private static final String TAG = "TypedUrlsTest";

    private static final String TYPED_URLS_TYPE = "Typed URLs";

    // TestHttpServerClient is preferred here but it can't be used. The test server
    // serves pages on localhost and Chrome doesn't sync localhost URLs as typed URLs.
    // This type of URL requires no external data connection or resources.
    private static final String URL = "data:text,testTypedUrl";

    // A container to store typed URL information for data verification.
    private static class TypedUrl {
        public final String id;
        public final String url;

        public TypedUrl(String id, String url) {
            this.id = id;
            this.url = url;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        setUpTestAccountAndSignInToSync();
        // Make sure the initial state is clean.
        assertClientTypedUrlCount(0);
        assertServerTypedUrlCountWithName(0, URL);
    }

    // Test syncing a typed URL from client to server.
    @LargeTest
    @Feature({"Sync"})
    public void testUploadTypedUrl() throws Exception {
        loadUrlByTyping(URL);
        waitForClientTypedUrlCount(1);
        waitForServerTypedUrlCountWithName(1, URL);
    }

    // Test syncing a typed URL from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadTypedUrl() throws Exception {
        addServerTypedUrl(URL);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);

        // Verify data synced to client.
        List<TypedUrl> typedUrls = getClientTypedUrls();
        assertEquals("Only the injected typed URL should exist on the client.",
                1, typedUrls.size());
        TypedUrl typedUrl = typedUrls.get(0);
        assertEquals("The wrong URL was found for the typed URL.", URL, typedUrl.url);
    }

    // Test syncing a typed URL deletion from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadDeletedTypedUrl() throws Exception {
        // Add the entity to test deleting.
        addServerTypedUrl(URL);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        assertServerTypedUrlCountWithName(1, URL);
        assertClientTypedUrlCount(1);

        // Delete on server, sync, and verify deleted locally.
        TypedUrl typedUrl = getClientTypedUrls().get(0);
        mFakeServerHelper.deleteEntity(typedUrl.id);
        waitForServerTypedUrlCountWithName(0, URL);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        waitForClientTypedUrlCount(0);
    }

    private void loadUrlByTyping(final String url) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                LoadUrlParams params = new LoadUrlParams(url, PageTransition.TYPED);
                getActivity().getActivityTab().loadUrl(params);
            }
        });
    }

    private void addServerTypedUrl(String url) throws InterruptedException {
        EntitySpecifics specifics = new EntitySpecifics();
        specifics.typedUrl = new TypedUrlSpecifics();
        specifics.typedUrl.url = url;
        specifics.typedUrl.title = url;
        specifics.typedUrl.visits = new long[]{1L};
        specifics.typedUrl.visitTransitions = new int[]{SyncEnums.TYPED};
        mFakeServerHelper.injectUniqueClientEntity(url /* name */, specifics);
    }

    private List<TypedUrl> getClientTypedUrls() throws JSONException {
        List<Pair<String, JSONObject>> rawTypedUrls = SyncTestUtil.getLocalData(
                mContext, TYPED_URLS_TYPE);
        List<TypedUrl> typedUrls = new ArrayList<TypedUrl>(rawTypedUrls.size());
        for (Pair<String, JSONObject> rawTypedUrl : rawTypedUrls) {
            String id =  rawTypedUrl.first;
            typedUrls.add(new TypedUrl(id, rawTypedUrl.second.getString("url")));
        }
        return typedUrls;
    }

    private void assertClientTypedUrlCount(int count) throws JSONException {
        assertEquals("There should be " + count + " local typed URL entities.",
                count, SyncTestUtil.getLocalData(mContext, TYPED_URLS_TYPE).size());
    }

    private void assertServerTypedUrlCountWithName(int count, String name) {
        assertTrue("Expected " + count + " server typed URLs with name " + name + ".",
                mFakeServerHelper.verifyEntityCountByTypeAndName(
                        count, ModelType.TYPED_URLS, name));
    }

    private void waitForClientTypedUrlCount(final int count) throws InterruptedException {
        boolean success = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return SyncTestUtil.getLocalData(mContext, TYPED_URLS_TYPE).size() == count;
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Expected " + count + " local typed URL entities.", success);
    }

    private void waitForServerTypedUrlCountWithName(final int count, final String name)
            throws InterruptedException {
        boolean success = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return mFakeServerHelper.verifyEntityCountByTypeAndName(
                            count, ModelType.TYPED_URLS, name);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Expected " + count + " server typed URLs with name " + name + ".", success);
    }
}
