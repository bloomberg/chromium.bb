// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmark;

import android.content.Context;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBrowserProvider.BookmarkNode;
import org.chromium.chrome.browser.ChromeBrowserProviderClient;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;

import java.util.Locale;
import java.util.concurrent.Callable;

/**
 * Tests the ManageBookmarkActivity, which allows users to add and edit bookmarks.
 */
@CommandLineFlags.Add(ChromeSwitches.ENABLE_ENHANCED_BOOKMARKS + "=0")
public class ManageBookmarkActivityTest extends ChromeTabbedActivityTestBase {
    private static final String DOM_DISTILLER_SCHEME = "chrome-distiller";

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();

        // Normally the data is cleared on start up to allow debugging, but adding bookmarks
        // via the test prevents adding them in the phone so we need to also clear the app data
        // on shutdown.
        // TODO(tedchoc): figure out why.
        //
        // Disable for debugging a particular test.
        ApplicationTestUtils.clearAppData(getInstrumentation().getTargetContext());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

    private String getMobileBookmarkFolderName() {
        Context context = getActivity();
        // Purposely not run on the UI thread as the ChromeBrowserProvider does not allow
        // execution on the UI thread.
        long mobileBookmarkFolderId =
                ChromeBrowserProviderClient.getMobileBookmarksFolderId(context);
        assertNotSame("Mobile bookmark folder ID returned as INVALID",
                ChromeBrowserProviderClient.INVALID_BOOKMARK_ID, mobileBookmarkFolderId);
        BookmarkNode node = ChromeBrowserProviderClient.getBookmarkNode(
                context, mobileBookmarkFolderId, ChromeBrowserProviderClient.GET_NODE);
        assertNotNull("Mobile bookmark node was null.", node);
        return node.name();
    }

    private void assertFolderText(final AddEditBookmarkFragment addEditFragment,
            String expectedText) {
        String actualTextContainer = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<String>() {
                    @Override
                    public String call() {
                        Button button = (Button) addEditFragment.getView().findViewById(
                                R.id.bookmark_folder_select);
                        return button.getText().toString();
                    }
                });
        assertEquals(expectedText, actualTextContainer);
    }

    @SmallTest
    @Feature({"Bookmarks", "Main"})
    public void testAddBookmark() throws InterruptedException {
        ManageBookmarkActivity addActivity = BookmarkTestUtils
                .selectBookmarkItemFromMenu(getInstrumentation(), getActivity());
        final AddEditBookmarkFragment addFragment = BookmarkTestUtils.loadAddEditFragment(
                addActivity);
        BookmarkTestUtils.clickOkButton(this, addFragment);
        final ManageBookmarkActivity editActivity = BookmarkTestUtils
                .selectBookmarkItemFromMenu(getInstrumentation(), getActivity());
        BookmarkTestUtils.loadAddEditFragment(editActivity);
        assertTrue("Expected "
                + "title to contain 'edit' when changing an existing bookmark",
                ((TextView) editActivity.findViewById(R.id.bookmark_action_title))
                        .getText().toString().toLowerCase(Locale.US).contains("edit"));
    }

    @SmallTest
    @Feature({"Bookmarks"})
    public void testAddInvalidBookmark() throws InterruptedException {
        // Tests that we do not crash while adding a bookmark with an invalid url.
        ManageBookmarkActivity addActivity = BookmarkTestUtils
                .selectBookmarkItemFromMenu(getInstrumentation(), getActivity());
        final AddEditBookmarkFragment addFragment = BookmarkTestUtils.loadAddEditFragment(
                addActivity);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ((EditText) addFragment.getView().findViewById(R.id.bookmark_url_input))
                        .setText("INVALID_URL");
            }
        });
        BookmarkTestUtils.clickOkButton(this, addFragment);
        getInstrumentation().waitForIdleSync();
    }

    @SmallTest
    @Feature({"Bookmarks"})
    public void testMobileFolderIsDefault() throws InterruptedException {
        String mobileBookmarksName = getMobileBookmarkFolderName();

        // Initial "Add Bookmark" ui should have default
        ManageBookmarkActivity addActivity = BookmarkTestUtils
                .selectBookmarkItemFromMenu(getInstrumentation(), getActivity());
        AddEditBookmarkFragment addFragment = BookmarkTestUtils.loadAddEditFragment(addActivity);
        assertFolderText(addFragment, mobileBookmarksName);

        BookmarkTestUtils.clickSelectFolderButton(this, addFragment);
        getInstrumentation().waitForIdleSync();
        final SelectBookmarkFolderFragment selectedFolder = BookmarkTestUtils.loadSelectFragment(
                addActivity);

        BookmarkTestUtils.clickNewFolderButton(this, selectedFolder);
        getInstrumentation().waitForIdleSync();
        final AddEditBookmarkFragment addNewFolderFragment = BookmarkTestUtils
                .loadAddFolderFragment(addActivity);
        assertFolderText(addNewFolderFragment, mobileBookmarksName);
    }

    @DisabledTest // Fails on android-one: crbug.com/540703
    @SmallTest
    @Feature({"Bookmarks"})
    public void testAddFolder() throws InterruptedException {
        ManageBookmarkActivity addActivity = BookmarkTestUtils
                .selectBookmarkItemFromMenu(getInstrumentation(), getActivity());
        final AddEditBookmarkFragment addFragment =
                BookmarkTestUtils.loadAddEditFragment(addActivity);

        BookmarkTestUtils.clickSelectFolderButton(this, addFragment);
        getInstrumentation().waitForIdleSync();
        SelectBookmarkFolderFragment selectedFolder = BookmarkTestUtils.loadSelectFragment(
                addActivity);

        BookmarkTestUtils.clickNewFolderButton(this, selectedFolder);
        getInstrumentation().waitForIdleSync();
        final AddEditBookmarkFragment addNewFolderFragment =
                BookmarkTestUtils.loadAddFolderFragment(addActivity);

        final String testFolderName = "Test Folder";
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ((EditText) addNewFolderFragment.getView().findViewById(R.id.bookmark_title_input))
                        .setText(testFolderName);
            }
        });
        BookmarkTestUtils.clickOkButton(this, addNewFolderFragment);
        assertFolderText(addFragment, testFolderName);
    }

    /**
     * Add bookmark in reader mode and see if the saved URL is the original one.
     * TODO(wychen): test enhanced bookmark as well?
     */
    // @SmallTest
    // @Feature({"Bookmarks"})
    // crbug.com/464794
    @FlakyTest
    public void testAddReaderModeBookmark() throws InterruptedException {
        String url = TestHttpServerClient.getUrl("chrome/test/data/android/google.html");
        String distillerUrl =
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(DOM_DISTILLER_SCHEME, url);
        loadUrl(distillerUrl);
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), distillerUrl);

        ManageBookmarkActivity addActivity = BookmarkTestUtils
                .selectBookmarkItemFromMenu(getInstrumentation(), getActivity());
        BookmarkTestUtils.loadAddEditFragment(addActivity);

        assertEquals(url,
                ((TextView) addActivity.findViewById(R.id.bookmark_url_input))
                        .getText().toString());
    }

    /**
     * Add an existing bookmark in reader mode and see if it is recognized as existing.
     */
    // @SmallTest
    // @Feature({"Bookmarks"})
    // crbug.com/464794
    @FlakyTest
    public void testAddExistingReaderModeBookmark() throws InterruptedException {
        String url = TestHttpServerClient.getUrl("chrome/test/data/android/google.html");
        String distillerUrl =
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(DOM_DISTILLER_SCHEME, url);
        loadUrl(distillerUrl);
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), distillerUrl);

        ManageBookmarkActivity addActivity = BookmarkTestUtils
                .selectBookmarkItemFromMenu(getInstrumentation(), getActivity());
        final AddEditBookmarkFragment addFragment = BookmarkTestUtils.loadAddEditFragment(
                addActivity);

        assertTrue("Expected "
                + "title to contain 'add' when adding a new bookmark",
                ((TextView) addActivity.findViewById(R.id.bookmark_action_title))
                        .getText().toString().toLowerCase(Locale.US).contains("add"));
        BookmarkTestUtils.clickOkButton(this, addFragment);


        final ManageBookmarkActivity editActivity = BookmarkTestUtils
                .selectBookmarkItemFromMenu(getInstrumentation(), getActivity());
        BookmarkTestUtils.loadAddEditFragment(editActivity);

        assertTrue("Expected "
                + "title to contain 'edit' when changing an existing bookmark",
                ((TextView) editActivity.findViewById(R.id.bookmark_action_title))
                        .getText().toString().toLowerCase(Locale.US).contains("edit"));
    }
}
