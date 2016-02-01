// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmark;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeBrowserProvider;

import java.util.Arrays;
import java.util.Date;

/**
 * Tests the use of the Bookmark URI as part of the Android provider public API.
 */
public class ProviderBookmarksUriTest extends ProviderTestBase {
    private static final String TAG = "ProviderBookmarkUriTest";
    private static final byte[] FAVICON_DATA = { 1, 2, 3 };

    private Uri mBookmarksUri;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mBookmarksUri = ChromeBrowserProvider.getBookmarksApiUri(getActivity());
        getContentResolver().delete(mBookmarksUri, null, null);
    }

    @Override
    protected void tearDown() throws Exception {
        getContentResolver().delete(mBookmarksUri, null, null);
        super.tearDown();
    }

    private Uri addBookmark(String url, String title, long lastVisitTime, long created, int visits,
            byte[] icon, int isBookmark) {
        ContentValues values = new ContentValues();
        values.put(BookmarkColumns.BOOKMARK, isBookmark);
        values.put(BookmarkColumns.DATE, lastVisitTime);
        values.put(BookmarkColumns.CREATED, created);
        values.put(BookmarkColumns.FAVICON, icon);
        values.put(BookmarkColumns.URL, url);
        values.put(BookmarkColumns.VISITS, visits);
        values.put(BookmarkColumns.TITLE, title);
        return getContentResolver().insert(mBookmarksUri, values);
    }

    @MediumTest
    @Feature({"Android-ContentProvider"})
    public void testAddBookmark() {
        final long lastUpdateTime = System.currentTimeMillis();
        final long createdTime = lastUpdateTime - 1000 * 60 * 60;
        final String url = "http://www.google.com/";
        final int visits = 2;
        final String title = "Google";
        ContentValues values = new ContentValues();
        values.put(BookmarkColumns.BOOKMARK, 0);
        values.put(BookmarkColumns.DATE, lastUpdateTime);
        values.put(BookmarkColumns.CREATED, createdTime);
        values.put(BookmarkColumns.FAVICON, FAVICON_DATA);
        values.put(BookmarkColumns.URL, url);
        values.put(BookmarkColumns.VISITS, visits);
        values.put(BookmarkColumns.TITLE, title);
        Uri uri = getContentResolver().insert(mBookmarksUri, values);
        Cursor cursor = getContentResolver().query(uri, null, null, null, null);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToNext());
        int index = cursor.getColumnIndex(BookmarkColumns.BOOKMARK);
        assertTrue(-1 != index);
        assertEquals(0, cursor.getInt(index));
        index = cursor.getColumnIndex(BookmarkColumns.CREATED);
        assertTrue(-1 != index);
        assertEquals(createdTime, cursor.getLong(index));
        index = cursor.getColumnIndex(BookmarkColumns.DATE);
        assertTrue(-1 != index);
        assertEquals(lastUpdateTime, cursor.getLong(index));
        index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
        assertTrue(-1 != index);
        assertTrue(byteArraysEqual(FAVICON_DATA, cursor.getBlob(index)));
        index = cursor.getColumnIndex(BookmarkColumns.URL);
        assertTrue(-1 != index);
        assertEquals(url, cursor.getString(index));
        index = cursor.getColumnIndex(BookmarkColumns.VISITS);
        assertTrue(-1 != index);
        assertEquals(visits, cursor.getInt(index));
    }

    @MediumTest
    @Feature({"Android-ContentProvider"})
    public void testQueryBookmark() {
        final long now = System.currentTimeMillis();
        final long lastUpdateTime[] = { now, now - 1000 * 60 };
        final long createdTime[] = { now - 1000 * 60 * 60, now - 1000 * 60 * 60 * 60 };
        final String url[] = { "http://www.google.com/", "http://mail.google.com/" };
        final int visits[] = { 2, 20 };
        final String title[] = { "Google", "Mail" };
        final int isBookmark[] = { 1, 0 };
        Uri[] uris = new Uri[2];
        byte[][] icons = { FAVICON_DATA, null };
        for (int i = 0; i < uris.length; i++) {
            uris[i] = addBookmark(url[i], title[i], lastUpdateTime[i], createdTime[i], visits[i],
                    icons[i], isBookmark[i]);
            assertNotNull(uris[i]);
        }

        // Query the 1st row.
        String[] selectionArgs = { url[0], String.valueOf(lastUpdateTime[0]),
                String.valueOf(visits[0]), String.valueOf(isBookmark[0]) };
        Cursor cursor = getContentResolver().query(mBookmarksUri, null,
                BookmarkColumns.URL + " = ? AND " + BookmarkColumns.DATE + " = ? AND "
                + BookmarkColumns.VISITS + " = ? AND " + BookmarkColumns.BOOKMARK + " = ? AND "
                + BookmarkColumns.FAVICON + " IS NOT NULL",
                selectionArgs, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToNext());
        int index = cursor.getColumnIndex(BookmarkColumns.BOOKMARK);
        assertTrue(-1 != index);
        assertEquals(isBookmark[0], cursor.getInt(index));
        index = cursor.getColumnIndex(BookmarkColumns.CREATED);
        assertTrue(-1 != index);
        assertEquals(createdTime[0], cursor.getLong(index));
        index = cursor.getColumnIndex(BookmarkColumns.DATE);
        assertTrue(-1 != index);
        assertEquals(lastUpdateTime[0], cursor.getLong(index));
        index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
        assertTrue(-1 != index);
        assertTrue(byteArraysEqual(icons[0], cursor.getBlob(index)));
        index = cursor.getColumnIndex(BookmarkColumns.URL);
        assertTrue(-1 != index);
        assertEquals(url[0], cursor.getString(index));
        index = cursor.getColumnIndex(BookmarkColumns.VISITS);
        assertTrue(-1 != index);
        assertEquals(visits[0], cursor.getInt(index));

        // Query the 2nd row.
        String[] selectionArgs2 = { url[1], String.valueOf(lastUpdateTime[1]),
                String.valueOf(visits[1]), String.valueOf(isBookmark[1]) };
        cursor = getContentResolver().query(mBookmarksUri, null,
                BookmarkColumns.URL + " = ? AND " + BookmarkColumns.DATE + " = ? AND "
                + BookmarkColumns.VISITS + " = ? AND " + BookmarkColumns.BOOKMARK + " = ? AND "
                + BookmarkColumns.FAVICON + " IS NULL",
                selectionArgs2, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToNext());
        index = cursor.getColumnIndex(BookmarkColumns.BOOKMARK);
        assertTrue(-1 != index);
        assertEquals(isBookmark[1], cursor.getInt(index));
        index = cursor.getColumnIndex(BookmarkColumns.CREATED);
        assertTrue(-1 != index);
        assertEquals(createdTime[1], cursor.getLong(index));
        index = cursor.getColumnIndex(BookmarkColumns.DATE);
        assertTrue(-1 != index);
        assertEquals(lastUpdateTime[1], cursor.getLong(index));
        index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
        assertTrue(-1 != index);
        assertTrue(byteArraysEqual(icons[1], cursor.getBlob(index)));
        index = cursor.getColumnIndex(BookmarkColumns.URL);
        assertTrue(-1 != index);
        assertEquals(url[1], cursor.getString(index));
        index = cursor.getColumnIndex(BookmarkColumns.VISITS);
        assertTrue(-1 != index);
        assertEquals(visits[1], cursor.getInt(index));
    }

    @MediumTest
    @Feature({"Android-ContentProvider"})
    public void testUpdateBookmark() {
        final long now = System.currentTimeMillis();
        final long lastUpdateTime[] = { now, now - 1000 * 60 };
        final long createdTime[] = { now - 1000 * 60 * 60, now - 1000 * 60 * 60 * 60 };
        final String url[] = { "http://www.google.com/", "http://mail.google.com/" };
        final int visits[] = { 2, 20 };
        final String title[] = { "Google", "Mail" };
        final int isBookmark[] = { 1, 0 };

        byte[][] icons = { FAVICON_DATA, null };
        Uri uri = addBookmark(url[0], title[0], lastUpdateTime[0], createdTime[0], visits[0],
                icons[0], isBookmark[0]);
        assertNotNull(uri);

        ContentValues values = new ContentValues();
        values.put(BookmarkColumns.BOOKMARK, isBookmark[1]);
        values.put(BookmarkColumns.DATE, lastUpdateTime[1]);
        values.put(BookmarkColumns.URL, url[1]);
        values.putNull(BookmarkColumns.FAVICON);
        values.put(BookmarkColumns.TITLE, title[1]);
        values.put(BookmarkColumns.VISITS, visits[1]);
        String[] selectionArgs = { String.valueOf(lastUpdateTime[0]),
                String.valueOf(isBookmark[0]) };
        getContentResolver().update(uri, values, BookmarkColumns.FAVICON +  " IS NOT NULL AND "
                + BookmarkColumns.DATE + "= ? AND " + BookmarkColumns.BOOKMARK + " = ?",
                selectionArgs);
        Cursor cursor = getContentResolver().query(uri, null, null, null, null);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToNext());
        int index = cursor.getColumnIndex(BookmarkColumns.BOOKMARK);
        assertTrue(-1 != index);
        assertEquals(isBookmark[1], cursor.getInt(index));
        index = cursor.getColumnIndex(BookmarkColumns.CREATED);
        assertTrue(-1 != index);
        assertEquals(createdTime[0], cursor.getLong(index));
        index = cursor.getColumnIndex(BookmarkColumns.DATE);
        assertTrue(-1 != index);
        assertEquals(lastUpdateTime[1], cursor.getLong(index));
        index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
        assertTrue(-1 != index);
        assertTrue(byteArraysEqual(icons[1], cursor.getBlob(index)));
        index = cursor.getColumnIndex(BookmarkColumns.URL);
        assertTrue(-1 != index);
        assertEquals(url[1], cursor.getString(index));
        index = cursor.getColumnIndex(BookmarkColumns.VISITS);
        assertTrue(-1 != index);
        assertEquals(visits[1], cursor.getInt(index));
    }

    @MediumTest
    @Feature({"Android-ContentProvider"})
    public void testDeleteBookmark() {
        final long now = System.currentTimeMillis();
        final long lastUpdateTime[] = { now, now - 1000 * 60 };
        final long createdTime[] = { now - 1000 * 60 * 60, now - 1000 * 60 * 60 * 60 };
        final String url[] = { "http://www.google.com/", "http://mail.google.com/" };
        final int visits[] = { 2, 20 };
        final String title[] = { "Google", "Mail" };
        final int isBookmark[] = { 1, 0 };
        Uri[] uris = new Uri[2];
        byte[][] icons = { FAVICON_DATA, null };
        for (int i = 0; i < uris.length; i++) {
            uris[i] = addBookmark(url[i], title[i], lastUpdateTime[i], createdTime[i], visits[i],
                    icons[i], isBookmark[i]);
            assertNotNull(uris[i]);
        }

        String[] selectionArgs = { String.valueOf(lastUpdateTime[0]),
                String.valueOf(isBookmark[0]) };
        getContentResolver().delete(mBookmarksUri, BookmarkColumns.FAVICON +  " IS NOT NULL AND "
                + BookmarkColumns.DATE + "= ? AND " + BookmarkColumns.BOOKMARK + " = ?",
                selectionArgs);
        Cursor cursor = getContentResolver().query(uris[0], null, null, null, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
        cursor = getContentResolver().query(uris[1], null, null, null, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        String[] selectionArgs1 = { String.valueOf(lastUpdateTime[1]),
                String.valueOf(isBookmark[1]) };
        getContentResolver().delete(mBookmarksUri, BookmarkColumns.FAVICON +  " IS NULL AND "
                + BookmarkColumns.DATE + "= ? AND " + BookmarkColumns.BOOKMARK + " = ?",
                selectionArgs1);
        cursor = getContentResolver().query(uris[1], null, null, null, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
    }

    /*
     * Copied from CTS test with minor adaptations.
     */
    @MediumTest
    @Feature({"Android-ContentProvider"})
    public void testBookmarksTable() {
        final String[] bookmarksProjection = new String[] {
                BookmarkColumns.ID, BookmarkColumns.URL, BookmarkColumns.VISITS,
                BookmarkColumns.DATE, BookmarkColumns.CREATED, BookmarkColumns.BOOKMARK,
                BookmarkColumns.TITLE, BookmarkColumns.FAVICON };
        final int idIndex = 0;
        final int urlIndex = 1;
        final int visitsIndex = 2;
        final int dataIndex = 3;
        final int createdIndex = 4;
        final int bookmarkIndex = 5;
        final int titleIndex = 6;
        final int faviconIndex = 7;

        final String insertBookmarkTitle = "bookmark_insert";
        final String insertBookmarkUrl = "www.bookmark_insert.com";

        final String updateBookmarkTitle = "bookmark_update";
        final String updateBookmarkUrl = "www.bookmark_update.com";

        // Test: insert.
        ContentValues value = new ContentValues();
        long createDate = new Date().getTime();
        value.put(BookmarkColumns.TITLE, insertBookmarkTitle);
        value.put(BookmarkColumns.URL, insertBookmarkUrl);
        value.put(BookmarkColumns.VISITS, 0);
        value.put(BookmarkColumns.DATE, createDate);
        value.put(BookmarkColumns.CREATED, createDate);
        value.put(BookmarkColumns.BOOKMARK, 0);

        Uri insertUri = getContentResolver().insert(mBookmarksUri, value);
        Cursor cursor = getContentResolver().query(
                mBookmarksUri,
                bookmarksProjection,
                BookmarkColumns.TITLE + " = ?",
                new String[] { insertBookmarkTitle },
                BookmarkColumns.DATE);
        assertTrue(cursor.moveToNext());
        assertEquals(insertBookmarkTitle, cursor.getString(titleIndex));
        assertEquals(insertBookmarkUrl, cursor.getString(urlIndex));
        assertEquals(0, cursor.getInt(visitsIndex));
        assertEquals(createDate, cursor.getLong(dataIndex));
        assertEquals(createDate, cursor.getLong(createdIndex));
        assertEquals(0, cursor.getInt(bookmarkIndex));
        // TODO(michaelbai): according to the test this should be null instead of an empty byte[].
        // BUG 6288508
        // assertTrue(cursor.isNull(FAVICON_INDEX));
        int Id = cursor.getInt(idIndex);
        cursor.close();

        // Test: update.
        value.clear();
        long updateDate = new Date().getTime();
        value.put(BookmarkColumns.TITLE, updateBookmarkTitle);
        value.put(BookmarkColumns.URL, updateBookmarkUrl);
        value.put(BookmarkColumns.VISITS, 1);
        value.put(BookmarkColumns.DATE, updateDate);

        getContentResolver().update(mBookmarksUri, value,
                BookmarkColumns.TITLE + " = ?",
                new String[] { insertBookmarkTitle });
        cursor = getContentResolver().query(
                mBookmarksUri,
                bookmarksProjection,
                BookmarkColumns.ID + " = " + Id,
                null, null);
        assertTrue(cursor.moveToNext());
        assertEquals(updateBookmarkTitle, cursor.getString(titleIndex));
        assertEquals(updateBookmarkUrl, cursor.getString(urlIndex));
        assertEquals(1, cursor.getInt(visitsIndex));
        assertEquals(updateDate, cursor.getLong(dataIndex));
        assertEquals(createDate, cursor.getLong(createdIndex));
        assertEquals(0, cursor.getInt(bookmarkIndex));
        // TODO(michaelbai): according to the test this should be null instead of an empty byte[].
        // BUG 6288508
        // assertTrue(cursor.isNull(FAVICON_INDEX));
        assertEquals(Id, cursor.getInt(idIndex));

        // Test: delete.
        getContentResolver().delete(insertUri, null, null);
        cursor = getContentResolver().query(
                mBookmarksUri,
                bookmarksProjection,
                BookmarkColumns.ID + " = " + Id,
                null, null);
        assertEquals(0, cursor.getCount());
    }

    /**
     * Checks if two byte arrays are equal. Used to compare icons.
     * @return True if equal, false otherwise.
     */
    private static boolean byteArraysEqual(byte[] byte1, byte[] byte2) {
        if (byte1 == null && byte2 != null) {
            return byte2.length == 0;
        }
        if (byte2 == null && byte1 != null) {
            return byte1.length == 0;
        }
        return Arrays.equals(byte1, byte2);
    }
}
