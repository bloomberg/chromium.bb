// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarkimport;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.test.mock.MockContentProvider;
import android.test.mock.MockContentResolver;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Log;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeBrowserProvider;
import org.chromium.chrome.browser.bookmark.BookmarkColumns;
import org.chromium.chrome.test.util.ApplicationData;
import org.chromium.chrome.test.util.BookmarkTestUtils;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.HashMap;
import java.util.HashSet;
import java.util.concurrent.TimeoutException;

/**
 * Tests importing bookmarks from Android Browser.
 */
public class AndroidBrowserImporterTest extends NativeLibraryTestBase {
    private static final String TAG = "BookmarkImporterTest";

    private static final String PUBLIC_PROVIDER = "browser";

    private static final String IS_BOOKMARK = "bookmark=1";
    private static final String HAS_URL = "url=?";
    private static final String BOOKMARK_MATCHES = IS_BOOKMARK + " AND "
            + BookmarkColumns.URL + "=? AND " + BookmarkColumns.TITLE + "=?";

    private static final String[] COLUMN_NAMES = new String[] {
        BookmarkColumns.BOOKMARK,
        BookmarkColumns.URL,
        BookmarkColumns.TITLE,
        BookmarkColumns.DATE,
        BookmarkColumns.CREATED,
        BookmarkColumns.VISITS,
        BookmarkColumns.FAVICON
    };

    // Bookmark source to mock.
    private enum BookmarksSource {
        NONE,
        PUBLIC_API
    }

    // Data of the mock bookmarks used for testing.
    private static class MockBookmark {
        public boolean isFolder;
        public String url;
        public String title;
        public Long lastVisit;
        public Long created;
        public Long visits;
        public byte[] favicon;

        public MockBookmark() {}

        public MockBookmark(boolean isFolder, String url, String title, Long lastVisit,
                Long created, Long visits, byte[] favicon) {
            this.isFolder = isFolder;
            this.url = url;
            this.title = title;
            this.lastVisit = lastVisit;
            this.created = created;
            this.visits = visits;
            this.favicon = favicon;
        }
    }

    private boolean mImportSucceeded;

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        ApplicationData.clearAppData(getInstrumentation().getTargetContext());
    }

    @Override
    public void setUp() {
        loadNativeLibraryAndInitBrowserProcess();
    }

    private AndroidBrowserImporter getBookmarkImporter(MockBookmark[] mockBookmarks,
            BookmarksSource source) {
        AndroidBrowserImporter importer =
                new AndroidBrowserImporter(getInstrumentation().getTargetContext());
        importer.setInputResolver(createMockResolver(mockBookmarks, source));
        if (source != BookmarksSource.NONE) {
            importer.setIgnoreAvailableProvidersForTestPurposes(true);
        }
        return importer;
    }

    private boolean importNewBookmarks(MockBookmark[] mockBookmarks, BookmarksSource source)
            throws InterruptedException {
        BookmarkImporter importer = getBookmarkImporter(mockBookmarks, source);

        // Note: the UI doesn't require any new bookmarks to consider the import as successful.
        // For testing purposes we do.
        final CallbackHelper importFinishedEvent = new CallbackHelper();
        importer.importBookmarks(new BookmarkImporter.OnBookmarksImportedListener() {
            @Override
            public void onBookmarksImported(BookmarkImporter.ImportResults results) {
                mImportSucceeded = results != null && results.newBookmarks > 0
                        && results.numImported == results.newBookmarks;
                importFinishedEvent.notifyCalled();
            }
        });
        try {
            importFinishedEvent.waitForCallback(0);
        } catch (TimeoutException e) {
            fail("Never received import finished event");
        }
        return mImportSucceeded;
    }

    private int countImportedNonFolderBookmarks() {
        Cursor cursor = getInstrumentation().getTargetContext().getContentResolver().query(
                ChromeBrowserProvider.getBookmarksApiUri(getInstrumentation().getTargetContext()),
                null, IS_BOOKMARK, null, null);
        int count = cursor.getCount();
        cursor.close();
        return count;
    }

    // TODO(leandrogracia): add support for hierarchical matching. This is likely to require changes
    // in ChromeBrowserProvider that should be addressed by a later patch.
    private boolean matchFlattenedImportedBookmarks(MockBookmark[] mockBookmarks) {
        Cursor cursor = getInstrumentation().getTargetContext().getContentResolver().query(
                ChromeBrowserProvider.getBookmarksApiUri(getInstrumentation().getTargetContext()),
                null, IS_BOOKMARK, null, null);
        if (mockBookmarks == null && cursor == null) return true;

        // Read all the imports to avoid problems with their order.
        HashMap<String, MockBookmark> importedBookmarks = new HashMap<String, MockBookmark>();
        assertTrue(cursor != null);
        while (cursor.moveToNext()) {
            MockBookmark bookmark = new MockBookmark();
            int index = cursor.getColumnIndex(BookmarkColumns.URL);
            if (index != -1 && !cursor.isNull(index)) bookmark.url = cursor.getString(index);

            index = cursor.getColumnIndex(BookmarkColumns.TITLE);
            if (index != -1 && !cursor.isNull(index)) bookmark.title = cursor.getString(index);

            index = cursor.getColumnIndex(BookmarkColumns.CREATED);
            if (index != -1 && !cursor.isNull(index)) bookmark.created = cursor.getLong(index);

            index = cursor.getColumnIndex(BookmarkColumns.DATE);
            if (index != -1 && !cursor.isNull(index)) bookmark.lastVisit = cursor.getLong(index);

            index = cursor.getColumnIndex(BookmarkColumns.VISITS);
            if (index != -1 && !cursor.isNull(index)) bookmark.visits = cursor.getLong(index);

            index = cursor.getColumnIndex(BookmarkColumns.FAVICON);
            if (index != -1 && !cursor.isNull(index)) bookmark.favicon = cursor.getBlob(index);

            // URLs should be unique in our model.
            assertFalse(importedBookmarks.containsKey(bookmark.url));
            importedBookmarks.put(bookmark.url, bookmark);
        }
        cursor.close();

        // Auxiliary class to get bookmark mismatch errors.
        class BookmarkMatchingException extends Exception {
            public BookmarkMatchingException(String message) {
                super(message);
            }
        }

        HashSet<String> matchingBookmarks = new HashSet<String>();
        final long now = System.currentTimeMillis();
        try {
            for (MockBookmark expected : mockBookmarks) {
                // Folders are not returned by our provider (not part of the public API yet).
                if (expected.isFolder) continue;

                // Matching can't be performed if the expectation doesn't have a URL.
                if (expected.url == null) {
                    throw new BookmarkMatchingException("Expected bookmark without a URL found.");
                }

                // Get the corresponding imported bookmark by its URL. If multiple entries in the
                // mock bookmarks with the same URL are found, all but the first will be skipped.
                if (matchingBookmarks.contains(expected.url)) continue;

                MockBookmark imported = importedBookmarks.get(expected.url);
                if (imported == null) {
                    throw new BookmarkMatchingException("Bookmark with URL " + expected.url
                            + " not found");
                }

                // Imported data must have a valid title despite what the expectation says.
                if (imported.title == null) {
                    throw new BookmarkMatchingException("Bookmark without title found: "
                            + imported.url);
                } else if (!imported.title.equals(expected.title)) {
                    throw new BookmarkMatchingException("Title doesn't match: " + expected.title
                            + " != " + imported.title);
                }

                // If not explicitely set in the expectations, the creation date should be available
                // after importing with a value not in the future.
                if (expected.created != null) {
                    if (!expected.created.equals(imported.created)) {
                        throw new BookmarkMatchingException("Creation timestamp doesn't match: "
                                + expected.created.toString() + " != "
                                + (imported.created != null ? imported.created.toString()
                                        : "null"));
                    }
                } else {
                    if (imported.created == null) {
                        throw new BookmarkMatchingException("Creation date not initialised.");
                    } else if (imported.created.longValue() > now) {
                        throw new BookmarkMatchingException("Creation set in the future");
                    }
                }

                // If not explicitely set in the expectations, the last visit should be available
                // after importing with a value not in the future.
                if (expected.lastVisit != null) {
                    if (!expected.lastVisit.equals(imported.lastVisit)) {
                        throw new BookmarkMatchingException("Last visit timestamp doesn't match: "
                                + expected.lastVisit.toString() + " != "
                                + (imported.lastVisit != null ? imported.lastVisit.toString()
                                        : "null"));
                    }
                } else {
                    if (imported.lastVisit == null) {
                        throw new BookmarkMatchingException("Last visit date not initialised.");
                    } else if (imported.lastVisit.longValue() > now) {
                        throw new BookmarkMatchingException("Last visit set in the future");
                    }
                }

                // Visits should be 1 if not explicitly set in the mock bookmark.
                if (expected.visits != null) {
                    if (!expected.visits.equals(imported.visits)) {
                        throw new BookmarkMatchingException("Number of visits doesn't match: "
                                + expected.visits.toString() + " != "
                                + (imported.visits != null ? imported.visits.toString() : "null"));
                    }
                } else {
                    if (imported.visits == null) {
                        throw new BookmarkMatchingException("Visit count not initialised.");
                    } else if (imported.visits.longValue() != 0) {
                        throw new BookmarkMatchingException("Invalid visit count initialization");
                    }
                }

                // If set, the favicons should be binary equals.
                if (!BookmarkTestUtils.byteArrayEqual(expected.favicon, imported.favicon)) {
                    throw new BookmarkMatchingException("Favicon doesn't match");
                }

                matchingBookmarks.add(expected.url);
            }

            // At the end the map and the set should have the same size.
            if (importedBookmarks.size() != matchingBookmarks.size()) {
                throw new BookmarkMatchingException(
                        "Not match could be found for all imported bookmarks: "
                        + matchingBookmarks.size() + " matched, should be "
                        + importedBookmarks.size());
            }
        } catch (BookmarkMatchingException e) {
            Log.w(TAG, e.getMessage());
            return false;
        }

        return true;
    }

    private void internalTestBookmarkFields(BookmarksSource source) throws InterruptedException {
        // Load a favicon for testing.
        byte[] favicon = BookmarkTestUtils.getIcon("chrome/test/data/android/favicon.png");
        assertTrue(favicon != null);

        // Create mock bookmark data to import. Test all non-hierarchy fields.
        final long now = System.currentTimeMillis();
        MockBookmark[] mockBookmarks = new MockBookmark[] {
            new MockBookmark(false, "http://www.google.com/", "Google", Long.valueOf(now), Long.valueOf(now - 100 * 60 * 60),
                    Long.valueOf(100), favicon),
            new MockBookmark(false, "http://maps.google.com/", "Google Maps", Long.valueOf(now - 10 * 60), Long.valueOf(now - 5 * 60 * 60),
                    Long.valueOf(20), null),
            new MockBookmark(false, "http://mail.google.com/", "Google Mail", null, null,
                    null, null)
        };

        // Import bookmarks and match the contents.
        assertTrue(importNewBookmarks(mockBookmarks, source));
        assertTrue(matchFlattenedImportedBookmarks(mockBookmarks));
    }

    private void internalTestIncompleteBookmarks(BookmarksSource source)
            throws InterruptedException {
        // Create incomplete mock bookmark data.
        MockBookmark[] mockBookmarks = new MockBookmark[] {
            new MockBookmark(false, "http://www.google.com/", null, null, null, null, null),
            new MockBookmark(false, null, "Google Maps", null, null, null, null)
        };

        // Should fail importing bookmarks.
        assertFalse(importNewBookmarks(mockBookmarks, source));
    }

    private void internalTestImportExisting(BookmarksSource source) throws InterruptedException {
        // One simple mock bookmark.
        MockBookmark[] mockBookmarks = new MockBookmark[] {
            new MockBookmark(false, "http://mail.google.com/", "Google Mail", null, null, null,
                    null)
        };

        // Import bookmarks. There should only be one imported.
        assertTrue(importNewBookmarks(mockBookmarks, source));
        assertEquals(1, countImportedNonFolderBookmarks());

        // Import the same bookmarks again. It should fail becase there aren't any new bookmarks.
        assertFalse(importNewBookmarks(mockBookmarks, source));
        assertEquals(1, countImportedNonFolderBookmarks());

        // Let's try adding something else.
        MockBookmark[] mockBookmarksExtra = new MockBookmark[] {
            new MockBookmark(false, "http://mail.google.com/", "Google Mail", null, null, null,
                    null),
            new MockBookmark(false, "http://maps.google.com/", "Google Maps", null, null, null,
                    null)
        };

        // The new bookmark should have been added.
        assertTrue(importNewBookmarks(mockBookmarksExtra, source));
        assertEquals(2, countImportedNonFolderBookmarks());
    }

    private void internalTestNoBookmarks(BookmarksSource source) throws InterruptedException {
        MockBookmark[] mockBookmarks = new MockBookmark[] {};
        assertFalse(importNewBookmarks(mockBookmarks, source));
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testBookmarkFieldsFromPublicAPI() throws InterruptedException {
        internalTestBookmarkFields(BookmarksSource.PUBLIC_API);
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testIncompleteBookmarksFromPublicAPI() throws InterruptedException {
        internalTestIncompleteBookmarks(BookmarksSource.PUBLIC_API);
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testImportExistingFromPublicAPI() throws InterruptedException {
        internalTestImportExisting(BookmarksSource.PUBLIC_API);
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testNoBookmarksInPublicAPI() throws InterruptedException {
        internalTestNoBookmarks(BookmarksSource.PUBLIC_API);
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testBookmarksAreAccessible() {
        MockBookmark[] mockBookmarks = new MockBookmark[] {
            new MockBookmark(false, "http://www.google.com/", "Google", null, null, null, null),
        };

        // Should not be accessible if no provider is reachable.
        assertFalse(getBookmarkImporter(mockBookmarks, BookmarksSource.NONE)
                .areBookmarksAccessible());

        // Should be accessible in all other cases.
        assertTrue(getBookmarkImporter(mockBookmarks, BookmarksSource.PUBLIC_API)
                .areBookmarksAccessible());
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testTimestampCorrection() throws InterruptedException {
        // Set the past and the future 1 hour away from now.
        final long lapse = 3600000;
        final Long now = Long.valueOf(System.currentTimeMillis());
        final Long past = Long.valueOf(Math.max(now.longValue() - lapse, 0));
        final Long future = Long.valueOf(now.longValue() + lapse);

        // Setting a maximum of 10 visits. Each visit means a database row
        // insertion in the provider, so very high values will make the test
        // fail because of the importing event timeout.
        final long maxVisits = 10;
        final Long pastForVisits = Long.valueOf(now.longValue() - maxVisits);
        final Long visitsValue = Long.valueOf(maxVisits * 100);

        MockBookmark[] mockBookmarks = new MockBookmark[] {
            // Last visit is in the future.
            new MockBookmark(false, "http://www.someweb1.com/", "Foo", future, null,
                    null, null),
            // Creation date is in the future.
            new MockBookmark(false, "http://www.someweb2.com/", "Foo", null, future,
                    null, null),
            // Creation date after last visit, both valid.
            new MockBookmark(false, "http://www.someweb3.com/", "Foo", past, now,
                    null, null),
            // Invalid number of visits for the lapse between creation and last visit.
            new MockBookmark(false, "http://www.someweb4.com/", "Foo", now, pastForVisits,
                    visitsValue, null),
        };

        // Timestamp correction should be independent to the source.
        assertTrue(importNewBookmarks(mockBookmarks, BookmarksSource.PUBLIC_API));
        assertEquals(mockBookmarks.length, countImportedNonFolderBookmarks());
        assertFalse(matchFlattenedImportedBookmarks(mockBookmarks));
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testVisitCountZeroAdjustment() throws InterruptedException {
        final Long now = Long.valueOf(System.currentTimeMillis());

        MockBookmark[] mockBookmarks = new MockBookmark[] {
            // Visit count is set to zero (required to be supported by the BrowserTest CTS).
            new MockBookmark(false, "http://www.zerovisits.com/", "Foo", now, now,
                    0L, null),
        };

        assertTrue(importNewBookmarks(mockBookmarks, BookmarksSource.PUBLIC_API));
        assertEquals(mockBookmarks.length, countImportedNonFolderBookmarks());
        // The imported bookmark should differ from mockBookmarks as visit count was ignored.
        assertFalse(matchFlattenedImportedBookmarks(mockBookmarks));
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testVisitCountOneAdjustment() throws InterruptedException {
        final Long now = Long.valueOf(System.currentTimeMillis());

        MockBookmark[] mockBookmarks = new MockBookmark[] {
            // Visit count is set to one (will be ignored).
            // See http://crbug.com/149376 for more details.
            new MockBookmark(false, "http://www.onevisit.com/", "Foo", now, now,
                    1L, null),
        };

        assertTrue(importNewBookmarks(mockBookmarks, BookmarksSource.PUBLIC_API));
        assertEquals(mockBookmarks.length, countImportedNonFolderBookmarks());
        assertTrue(matchFlattenedImportedBookmarks(mockBookmarks));
    }

    @MediumTest
    @Feature({"Bookmarks", "Import"})
    public void testVisitCountTwoAdjustment() throws InterruptedException {
        final Long now = Long.valueOf(System.currentTimeMillis());

        MockBookmark[] mockBookmarks = new MockBookmark[] {
            // Visit count is set to one (will be ignored).
            // See http://crbug.com/149376 for more details.
            new MockBookmark(false, "http://www.onevisit.com/", "Foo", now + 100, now,
                    1L, null),
        };

        assertTrue(importNewBookmarks(mockBookmarks, BookmarksSource.PUBLIC_API));
        assertEquals(mockBookmarks.length, countImportedNonFolderBookmarks());
        // The imported bookmark should differ from mockBookmarks as visit count was ignored.
        assertFalse(matchFlattenedImportedBookmarks(mockBookmarks));
    }

    private static class ImportMockContentProvider extends MockContentProvider {
        private final MockBookmark[] mMockData;
        private final boolean mFlatten;

        ImportMockContentProvider(Context context, MockBookmark[] data, boolean flatten) {
            super(context);
            mMockData = data;
            mFlatten = flatten;
        }

        @Override
        public Cursor query(Uri uri, String[] projection, String selection,
                String[] selectionArgs, String sortOrder) {
            // Provide a new cursor with the mock data.
            MatrixCursor cursor = new MatrixCursor(COLUMN_NAMES);
            for (MockBookmark bookmark : mMockData) {
                if (mFlatten && bookmark.isFolder) continue;

                // There are 3 kinds of queries performed by the iterators:
                // 1. Retrieving all bookmarks.
                // 2. Querying a specific URL in history.
                // 3. Looking for matching bookmarks (url and title) in the public API.
                if (selection.equals(HAS_URL)) {
                    assertTrue(selectionArgs != null);
                    assertEquals(1, selectionArgs.length);
                    if (bookmark.url != null && !bookmark.url.equals(selectionArgs[0])) {
                        continue;
                    }
                } else if (selection.equals(BOOKMARK_MATCHES)) {
                    assertTrue(selectionArgs != null);
                    assertEquals(2, selectionArgs.length);
                    if ((bookmark.url != null && !bookmark.url.equals(selectionArgs[0]))
                            || (bookmark.title != null
                                    && !bookmark.title.equals(selectionArgs[1]))) {
                        continue;
                    }
                }

                cursor.addRow(new Object[] {
                        1, bookmark.url, bookmark.title, bookmark.lastVisit,
                        bookmark.created, bookmark.visits, bookmark.favicon
                });

                // Should there be only one history URL result. No need to look for more.
                if (selection.equals(HAS_URL)) break;
            }
            cursor.move(0);
            return cursor;
        }

        @Override
        public Uri insert(Uri uri, ContentValues values) {
            // Proxy to the original content resolver.
            return getContext().getContentResolver().insert(uri, values);
        }
    }

    private MockContentResolver createMockResolver(MockBookmark[] mockBookmarks,
            BookmarksSource source) {
        MockContentResolver mockResolver = new MockContentResolver();
        switch (source) {
            case NONE:
                // No provider added. Used to test bookmark availability.
                break;

            case PUBLIC_API:
                mockResolver.addProvider(PUBLIC_PROVIDER,
                        new ImportMockContentProvider(getInstrumentation().getTargetContext(),
                                mockBookmarks, true));
                break;
        }
        return mockResolver;
    }
}
