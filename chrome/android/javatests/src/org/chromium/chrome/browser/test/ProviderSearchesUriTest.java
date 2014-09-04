// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test;

import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Browser;
import android.provider.Browser.SearchColumns;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeBrowserProvider;

import java.util.Date;

/**
 * Tests the use of the Searches URI as part of the Android provider public API.
 */
public class ProviderSearchesUriTest extends ProviderTestBase {

    private Uri mSearchesUri;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mSearchesUri = ChromeBrowserProvider.getSearchesApiUri(getActivity());
        getContentResolver().delete(mSearchesUri, null, null);
    }

    @Override
    protected void tearDown() throws Exception {
        getContentResolver().delete(mSearchesUri, null, null);
        super.tearDown();
    }

    private Uri addSearchTerm(String searchTerm, long searchTime) {
        ContentValues values = new ContentValues();
        values.put(SearchColumns.SEARCH, searchTerm);
        values.put(SearchColumns.DATE, searchTime);
        return getContentResolver().insert(mSearchesUri, values);
    }

    /**
     * @MediumTest
     * @Feature({"Android-ContentProvider"})
     * BUG 154683
     */
    @DisabledTest
    public void testAddSearchTerm() {
        long searchTime = System.currentTimeMillis();
        String searchTerm = "chrome";
        Uri uri = addSearchTerm(searchTerm, searchTime);
        assertNotNull(uri);
        String[] selectionArgs = { searchTerm, String.valueOf(searchTime) };
        Cursor cursor = getContentResolver().query(uri, null, SearchColumns.SEARCH + "=? AND " +
                SearchColumns.DATE + " = ? ", selectionArgs, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToNext());
        int index = cursor.getColumnIndex(SearchColumns.SEARCH);
        assertTrue(-1 != index);
        assertEquals(searchTerm, cursor.getString(index));
        index = cursor.getColumnIndex(SearchColumns.DATE);
        assertTrue(-1 != index);
        assertEquals(searchTime, cursor.getLong(index));
    }

    /**
     * @MediumTest
     * @Feature({"Android-ContentProvider"})
     * BUG 154683
     */
    @DisabledTest
    public void testUpdateSearchTerm() {
        long[] searchTime = { System.currentTimeMillis(), System.currentTimeMillis() - 1000 };
        String[] searchTerm = { "chrome", "chromium" };
        Uri uri = addSearchTerm(searchTerm[0], searchTime[0]);
        ContentValues values = new ContentValues();
        values.put(SearchColumns.SEARCH, searchTerm[1]);
        values.put(SearchColumns.DATE, searchTime[1]);
        getContentResolver().update(uri, values, null, null);
        String[] selectionArgs = { searchTerm[0] };
        Cursor cursor = getContentResolver().query(mSearchesUri, null, SearchColumns.SEARCH + "=?",
                selectionArgs, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
        String[] selectionArgs1 = { searchTerm[1] };
        cursor = getContentResolver().query(mSearchesUri, null, SearchColumns.SEARCH + "=?",
                selectionArgs1, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToNext());
        int index = cursor.getColumnIndex(SearchColumns.SEARCH);
        assertTrue(-1 != index);
        assertEquals(searchTerm[1], cursor.getString(index));
        index = cursor.getColumnIndex(SearchColumns.DATE);
        assertTrue(-1 != index);
        assertEquals(searchTime[1], cursor.getLong(index));
    }

    /**
     * @MediumTest
     * @Feature({"Android-ContentProvider"})
     * BUG 154683
     */
    @DisabledTest
    public void testDeleteSearchTerm() {
        long[] searchTime = { System.currentTimeMillis(), System.currentTimeMillis() - 1000 };
        String[] searchTerm = {"chrome", "chromium"};
        Uri uri[] = new Uri[2];
        for (int i = 0; i < uri.length; i++) {
            uri[i] = addSearchTerm(searchTerm[i], searchTime[i]);
        }
        getContentResolver().delete(uri[0], null, null);
        String[] selectionArgs = { searchTerm[0] };
        Cursor cursor = getContentResolver().query(mSearchesUri, null, SearchColumns.SEARCH + "=?",
                selectionArgs, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
        String[] selectionArgs1 = { searchTerm[1] };
        cursor = getContentResolver().query(mSearchesUri, null, SearchColumns.SEARCH + "=?",
                selectionArgs1, null);
        assertNotNull(cursor);
        assertEquals(1, cursor.getCount());
        assertTrue(cursor.moveToNext());
        int index = cursor.getColumnIndex(SearchColumns.SEARCH);
        assertTrue(-1 != index);
        assertEquals(searchTerm[1], cursor.getString(index));
        index = cursor.getColumnIndex(SearchColumns.DATE);
        assertTrue(-1 != index);
        assertEquals(searchTime[1], cursor.getLong(index));
        getContentResolver().delete(uri[1], null, null);
        cursor = getContentResolver().query(uri[1], null, null, null, null);
        assertNotNull(cursor);
        assertEquals(0, cursor.getCount());
    }

    // Copied from CTS test with minor adaptations.
    /**
     * @MediumTest
     * @Feature({"Android-ContentProvider"})
     * BUG 154683
     */
    @DisabledTest
    public void testSearchesTable() {
        final int idIndex = 0;
        String insertSearch = "search_insert";
        String updateSearch = "search_update";

        // Test: insert
        ContentValues value = new ContentValues();
        long createDate = new Date().getTime();
        value.put(SearchColumns.SEARCH, insertSearch);
        value.put(SearchColumns.DATE, createDate);

        Uri insertUri = getContentResolver().insert(mSearchesUri, value);
        Cursor cursor = getContentResolver().query(mSearchesUri,
                Browser.SEARCHES_PROJECTION, SearchColumns.SEARCH + " = ?",
                new String[] { insertSearch }, null);
        assertTrue(cursor.moveToNext());
        assertEquals(insertSearch,
                cursor.getString(Browser.SEARCHES_PROJECTION_SEARCH_INDEX));
        assertEquals(createDate,
                cursor.getLong(Browser.SEARCHES_PROJECTION_DATE_INDEX));
        int id = cursor.getInt(idIndex);
        cursor.close();

        // Test: update
        value.clear();
        long updateDate = new Date().getTime();
        value.put(SearchColumns.SEARCH, updateSearch);
        value.put(SearchColumns.DATE, updateDate);

        getContentResolver().update(mSearchesUri, value,
                SearchColumns._ID + " = " + id, null);
        cursor = getContentResolver().query(mSearchesUri,
                Browser.SEARCHES_PROJECTION,
                SearchColumns._ID + " = " + id, null, null);
        assertTrue(cursor.moveToNext());
        assertEquals(updateSearch,
                cursor.getString(Browser.SEARCHES_PROJECTION_SEARCH_INDEX));
        assertEquals(updateDate,
                cursor.getLong(Browser.SEARCHES_PROJECTION_DATE_INDEX));
        assertEquals(id, cursor.getInt(idIndex));

        // Test: delete
        getContentResolver().delete(insertUri, null, null);
        cursor = getContentResolver().query(mSearchesUri,
                Browser.SEARCHES_PROJECTION,
                SearchColumns._ID + " = " + id, null, null);
        assertEquals(0, cursor.getCount());
    }
}
