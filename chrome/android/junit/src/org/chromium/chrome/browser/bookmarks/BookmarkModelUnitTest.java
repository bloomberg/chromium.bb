// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.shadows.ShadowMultiDex;
import org.chromium.chrome.browser.offlinepages.ClientId;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

/**
 * Robolectric tests for {@link BookmarkUtils}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, application = BaseChromiumApplication.class,
        shadows = {ShadowMultiDex.class})
public class BookmarkModelUnitTest {
    @Test
    public void testGetBookmarkIdForOfflineClientIdBadId() {
        ClientId clientId = new ClientId("bookmark", "id");
        BookmarkId id = BookmarkModel.getBookmarkIdForOfflineClientId(clientId);

        assertEquals(id.getType(), BookmarkType.NORMAL);
        assertEquals(id.getId(), -1);
    }

    @Test
    public void testGetBookmarkIdForOfflineClientIdWrongNamespace() {
        ClientId clientId = new ClientId("wrong", "6");
        BookmarkId id = BookmarkModel.getBookmarkIdForOfflineClientId(clientId);

        assertEquals(id.getType(), BookmarkType.NORMAL);
        assertEquals(id.getId(), -1);
    }

    @Test
    public void testGetBookmarkIdForOfflineClientIdOk() {
        ClientId clientId = new ClientId("bookmark", "6");
        BookmarkId id = BookmarkModel.getBookmarkIdForOfflineClientId(clientId);

        assertEquals(id.getType(), BookmarkType.NORMAL);
        assertEquals(id.getId(), 6);
    }
}
