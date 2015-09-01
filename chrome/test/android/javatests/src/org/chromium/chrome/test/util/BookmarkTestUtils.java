// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static org.chromium.chrome.browser.bookmark.ManageBookmarkActivity.ADD_FOLDER_FRAGMENT_TAG;
import static org.chromium.chrome.browser.bookmark.ManageBookmarkActivity.BASE_ADD_EDIT_FRAGMENT_TAG;
import static org.chromium.chrome.browser.bookmark.ManageBookmarkActivity.BASE_SELECT_FOLDER_FRAGMENT_TAG;

import android.app.Instrumentation;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.BitmapFactory;
import android.test.InstrumentationTestCase;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import android.widget.TextView;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmark.AddEditBookmarkFragment;
import org.chromium.chrome.browser.bookmark.ManageBookmarkActivity;
import org.chromium.chrome.browser.bookmark.SelectBookmarkFolderFragment;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.UiUtils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Set of utility functions shared between tests managing bookmarks.
 */
public class BookmarkTestUtils {
    private static final String TAG = "BookmarkTestUtils";

    /**
     * Checks if two byte arrays are equal. Used to compare icons.
     * @return True if equal, false otherwise.
     */
    public static boolean byteArrayEqual(byte[] byte1, byte[] byte2) {
        if (byte1 == null && byte2 != null) {
            return byte2.length == 0;
        }
        if (byte2 == null && byte1 != null) {
            return byte1.length == 0;
        }
        return Arrays.equals(byte1, byte2);
    }

    /**
     * Retrieves a byte array with the decoded image data of an icon.
     * @return Data of the icon.
     */
    public static byte[] getIcon(String testPath) {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        try {
            InputStream faviconStream = (InputStream) (new URL(
                    TestHttpServerClient.getUrl(testPath))).getContent();
            Bitmap faviconBitmap = BitmapFactory.decodeStream(faviconStream);
            faviconBitmap.compress(CompressFormat.PNG, 0, bos);
        } catch (IOException e) {
            Log.e(TAG, "Error trying to get the icon '" + testPath + "': " + e.getMessage());
        }
        return bos.toByteArray();
    }

    /**
     * Opens the add/edit bookmark menu.
     */
    public static ManageBookmarkActivity selectBookmarkItemFromMenu(
            Instrumentation instrumentation, ChromeTabbedActivity mainActivity) {
        ManageBookmarkActivity activity = ActivityUtils
                .waitForActivity(
                        instrumentation,
                        ManageBookmarkActivity.class,
                        new MenuUtils.MenuActivityTrigger(
                                instrumentation, mainActivity, R.id.bookmark_this_page_id));
        instrumentation.waitForIdleSync();
        return activity;
    }

    /**
     * Clicks the Select Folder button in the given AddEditBookmark dialog.
     * @throws InterruptedException
     */
    public static void clickSelectFolderButton(
            InstrumentationTestCase instrumentationTestCase,
            AddEditBookmarkFragment addEditFragment) throws InterruptedException {
        CallbackHelper folderSelectCallback = new CallbackHelper();
        initializeAddEditCallback(addEditFragment, folderSelectCallback);
        TestTouchUtils.clickView(
                instrumentationTestCase,
                addEditFragment.getView().findViewById(R.id.bookmark_folder_select));
        try {
            folderSelectCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Folder select did not occur");
        }
    }

    /**
     * Clicks the New Folder button in the given SelectBookmarkFolder dialog.
     * @throws InterruptedException
     */
    public static void clickNewFolderButton(
            InstrumentationTestCase instrumentationTestCase,
            SelectBookmarkFolderFragment selectFragment) throws InterruptedException {
        CallbackHelper newFolderEvent = new CallbackHelper();
        initializeNewFolderCallback(selectFragment, newFolderEvent);
        TestTouchUtils.clickView(
                instrumentationTestCase,
                selectFragment.getView().findViewById(R.id.new_folder_btn));
        try {
            newFolderEvent.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Never received new folder event");
        }
    }

    /**
     * Asserts that the URL bar contains the expected URL.
     */
    public static void assertUrlBarEquals(ChromeTabbedActivity mainActivity, String msg,
            String expectedUrl) {
        final UrlBar urlBar = (UrlBar) mainActivity.findViewById(R.id.url_bar);
        // Ideally we should not be depending on UI to verify that navigation to a url was
        // successful. UrlBar behavior is different for http and https urls. We always show
        // the scheme for https:// urls but not for http://.
        String urlBarText = urlBar.getText().toString();
        if (!urlBarText.startsWith("https")) {
            urlBarText = "http://" + urlBarText;
        }
        Assert.assertEquals(msg, expectedUrl, urlBarText);
    }

    private static void initializeAddEditCallback(
            final AddEditBookmarkFragment addEditFragment,
            final CallbackHelper callback) {
        // Proxy the messages sent from the fragment to detect if asynchronous operations succeeded
        // before finishing the fragment.
        final AddEditBookmarkFragment.OnActionListener originalListener =
                addEditFragment.getOnActionListenerForTest();

        addEditFragment.setOnActionListener(new AddEditBookmarkFragment.OnActionListener() {
            @Override
            public void onCancel() {
                if (originalListener != null) {
                    originalListener.onCancel();
                }
            }

            @Override
            public void onNodeEdited(long id) {
                if (originalListener != null) {
                    originalListener.onNodeEdited(id);
                }
                // Insert / update operation succeeded.
                callback.notifyCalled();
            }

            @Override
            public void onFolderCreated(long id, String name) {
                if (originalListener != null) {
                    originalListener.onFolderCreated(id, name);
                }
                // Insert / update operation succeeded.
                callback.notifyCalled();
            }

            @Override
            public void triggerFolderSelection() {
                if (originalListener != null) {
                    originalListener.triggerFolderSelection();
                }
                // Bookmarks folder input button was clicked.
                callback.notifyCalled();
            }

            @Override
            public void onRemove() {
                if (originalListener != null) {
                    originalListener.onRemove();
                }
                // Delete operation succeeded.
                callback.notifyCalled();
            }

            @Override
            public void setBackEnabled(boolean enabled) {
                if (originalListener != null) {
                    originalListener.setBackEnabled(enabled);
                }
            }
        });
    }

    private static void initializeNewFolderCallback(
            final SelectBookmarkFolderFragment selectBookmarkFolderFragment,
            final CallbackHelper newFolderEvent) {
        // Proxy the messages sent from the fragment to detect if asynchronous operations succeeded
        // before finishing the fragment.
        final SelectBookmarkFolderFragment.OnActionListener originalListener =
                selectBookmarkFolderFragment.getOnActionListenerForTest();

        selectBookmarkFolderFragment.setOnActionListener(
                new SelectBookmarkFolderFragment.OnActionListener() {
                    @Override
                    public void triggerNewFolderCreation(
                            long selectedFolderId, String selectedFolderName) {
                        if (originalListener != null) {
                            originalListener.triggerNewFolderCreation(
                                    selectedFolderId, selectedFolderName);
                        }
                        // New folder button was clicked.
                        newFolderEvent.notifyCalled();
                    }
                });
    }

    public static AddEditBookmarkFragment loadAddEditFragment(
            ManageBookmarkActivity bookmarkActivity)
            throws InterruptedException {
        AddEditBookmarkFragment fragment = ActivityUtils.waitForFragment(
                bookmarkActivity, BASE_ADD_EDIT_FRAGMENT_TAG);
        waitForFoldersToLoad(fragment);
        return fragment;
    }

    public static AddEditBookmarkFragment loadAddFolderFragment(
            ManageBookmarkActivity bookmarkActivity)
            throws InterruptedException {
        AddEditBookmarkFragment fragment = ActivityUtils.waitForFragment(
                bookmarkActivity, ADD_FOLDER_FRAGMENT_TAG);
        waitForFoldersToLoad(fragment);
        return fragment;
    }

    public static SelectBookmarkFolderFragment loadSelectFragment(
            ManageBookmarkActivity bookmarkActivity) throws InterruptedException {
        SelectBookmarkFolderFragment fragment = ActivityUtils.waitForFragment(
                bookmarkActivity, BASE_SELECT_FOLDER_FRAGMENT_TAG);
        waitForFoldersToLoad(fragment);
        return fragment;
    }

    private static void waitForFoldersToLoad(final AddEditBookmarkFragment addFragment)
            throws InterruptedException {
        checkCriteria("Failed to load Fragment.", new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return !addFragment.isLoadingBookmarks();
            }
        });
    }

    private static void waitForFoldersToLoad(final SelectBookmarkFolderFragment selectedFolder)
            throws InterruptedException {
        checkCriteria("Failed to load Fragment.", new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return selectedFolder.areFoldersLoaded();
            }
        });
    }

    private static void checkCriteria(String message, final Callable<Boolean> criteria)
            throws InterruptedException {
        Assert.assertTrue(message, CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(criteria);
            }
        }));
    }

    /**
     * Navigates to a URL in the current tab and bookmarks it through the add/edit bookmark UI.
     * @throws InterruptedException
     */
    public static void addCurrentUrlAsBookmark(InstrumentationTestCase testCase,
            ChromeTabbedActivity mainActivity)
            throws InterruptedException {
        final ManageBookmarkActivity addActivity =
                selectBookmarkItemFromMenu(testCase.getInstrumentation(), mainActivity);
        final AddEditBookmarkFragment addFragment = loadAddEditFragment(addActivity);
        clickOkButton(testCase, addFragment);
    }

    /**
     * Returns a unique UI element that has the given description text.
     *
     * @param viewGroup    The group to which the view belongs.
     * @param expectedText The expected description text.
     * @return The unique view, if one exists. Throws an exception if one doesn't exist.
     */
    public static View getViewWithText(final ViewGroup viewGroup, final String expectedText) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<View>() {
            @Override
            public View call() throws Exception {
                ArrayList<View> outViews = new ArrayList<View>();
                ArrayList<View> matchingViews = new ArrayList<View>();
                viewGroup.findViewsWithText(outViews, expectedText, View.FIND_VIEWS_WITH_TEXT);
                // outViews includes all views whose text contains expectedText as a
                // case-insensitive substring. Filter these views to find only exact string matches.
                for (View v : outViews) {
                    if (TextUtils.equals(((TextView) v).getText().toString(), expectedText)) {
                        matchingViews.add(v);
                    }
                }
                Assert.assertEquals("Exactly one item should be present.", 1, matchingViews.size());
                return matchingViews.get(0);
            }
        });
    }

    /**
     * Click the folder with the title @param folderTitle in the SelectBookmarkFolder fragment.
     * @throws InterruptedException
     */
    public static void clickFolderItem(InstrumentationTestCase testCase,
            final SelectBookmarkFolderFragment fragment, String folderTitle)
            throws InterruptedException {
        final ListView foldersList = (ListView) fragment.getView().findViewById(
                R.id.bookmark_folder_list);
        TextView folderItem =
                (TextView) getViewWithText(foldersList, folderTitle);
        TestTouchUtils.clickView(testCase, folderItem);
        UiUtils.settleDownUI(testCase.getInstrumentation());
    }

    /**
     * Click OK button on the AddEditBookmarkFragment fragment and wait for operation to be
     * completed.
     * @throws InterruptedException
     */
    public static void clickOkButton(
            InstrumentationTestCase testCase, AddEditBookmarkFragment fragment)
            throws InterruptedException {
        CallbackHelper okCallback = new CallbackHelper();
        initializeAddEditCallback(fragment, okCallback);
        TestTouchUtils.clickView(testCase, fragment.getView().findViewById(R.id.ok));
        try {
            okCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("OK action never triggered an update");
        }
        testCase.getInstrumentation().waitForIdleSync();
    }
}
