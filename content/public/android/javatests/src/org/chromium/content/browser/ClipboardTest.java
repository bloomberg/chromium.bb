// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.TargetApi;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Build;
import android.test.suitebuilder.annotation.LargeTest;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.input.ImeAdapter;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.concurrent.Callable;

/**
 * Tests rich text clipboard functionality.
 */
public class ClipboardTest extends ContentShellTestBase {
    private static final String TEST_PAGE_DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><body>Hello, <a href=\"http://www.example.com/\">world</a>, how <b> " +
            "Chromium</b> doing today?</body></html>");

    private static final String EXPECTED_TEXT_RESULT = "Hello, world, how Chromium doing today?";

    // String to search for in the HTML representation on the clipboard.
    private static final String EXPECTED_HTML_NEEDLE = "http://www.example.com/";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        launchContentShellWithUrl(TEST_PAGE_DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
    }

    /**
     * Tests that copying document fragments will put at least a plain-text representation
     * of the contents on the clipboard. For Android JellyBean and higher, we also expect
     * the HTML representation of the fragment to be available.
     */
    @LargeTest
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    @Feature({"Clipboard","TextInput"})
    @RerunWithUpdatedContainerView
    public void testCopyDocumentFragment() throws Throwable {
        final ClipboardManager clipboardManager = (ClipboardManager)
                getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
        assertNotNull(clipboardManager);

        // Clear the clipboard to make sure we start with a clean state.
        clipboardManager.setPrimaryClip(ClipData.newPlainText(null, ""));
        assertFalse(hasPrimaryClip(clipboardManager));

        ImeAdapter adapter = getContentViewCore().getImeAdapterForTest();
        selectAll(adapter);
        copy(adapter);

        // Waits until data has been made available on the Android clipboard.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        return hasPrimaryClip(clipboardManager);
                    }
                });
            }
        }));

        // Verify that the data on the clipboard is what we expect it to be. For Android JB MR2
        // and higher we expect HTML content, for other versions the plain-text representation.
        final ClipData clip = clipboardManager.getPrimaryClip();
        assertEquals(EXPECTED_TEXT_RESULT, clip.getItemAt(0).coerceToText(getActivity()));

        // Android JellyBean and higher should have a HTML representation on the clipboard as well.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            String htmlText = clip.getItemAt(0).getHtmlText();

            assertNotNull(htmlText);
            assertTrue(htmlText.contains(EXPECTED_HTML_NEEDLE));
        }
    }

    private void copy(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.copy();
            }
        });
    }

    private void selectAll(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.selectAll();
            }
        });
    }

    // Returns whether there is a primary clip with content on the current clipboard.
    private Boolean hasPrimaryClip(ClipboardManager clipboardManager) {
        final ClipData clip = clipboardManager.getPrimaryClip();
        if (clip != null && clip.getItemCount() > 0) {
            return !TextUtils.isEmpty(clip.getItemAt(0).getText());
        }

        return false;
    }
}
