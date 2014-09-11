// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Integration tests for text selection-related behavior.
 */
public class ContentViewCoreSelectionTest extends ContentShellTestBase {
    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\"" +
            "content=\"width=device-width, initial-scale=1.1, maximum-scale=1.5\" /></head>" +
            "<body><form action=\"about:blank\">" +
            "<input id=\"empty_input_text\" type=\"text\" />" +
            "<br/><p><span id=\"plain_text_1\">This is Plain Text One</span></p>" +
            "<br/><p><span id=\"plain_text_2\">This is Plain Text Two</span></p>" +
            "<br/><input id=\"empty_input_text\" type=\"text\" />" +
            "<br/><input id=\"input_text\" type=\"text\" value=\"Sample Text\" />" +
            "<br/><textarea id=\"empty_textarea\" rows=\"2\" cols=\"20\"></textarea>" +
            "<br/><textarea id=\"textarea\" rows=\"2\" cols=\"20\">Sample Text</textarea>" +
            "</form></body></html>");

    private ContentViewCore mContentViewCore;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        mContentViewCore = getContentViewCore();
        assertWaitForPageScaleFactorMatch(1.1f);
        assertWaitForSelectActionBarVisible(false);
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionClearedAfterLossOfFocus() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);

        requestFocusOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertFalse(mContentViewCore.hasSelection());

        requestFocusOnUiThread(true);
        assertWaitForSelectActionBarVisible(false);
        assertFalse(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterLossOfFocusIfRequested() throws Throwable {
        requestFocusOnUiThread(true);

        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());

        mContentViewCore.preserveSelectionOnNextLossOfFocus();
        requestFocusOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertTrue(mContentViewCore.hasSelection());

        requestFocusOnUiThread(true);
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());

        // Losing focus yet again should properly clear the selection.
        requestFocusOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertFalse(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterReshown() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());

        setVisibileOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertTrue(mContentViewCore.hasSelection());

        setVisibileOnUiThread(true);
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextSelection"})
    public void testSelectionPreservedAfterReattached() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());

        setAttachedOnUiThread(false);
        assertWaitForSelectActionBarVisible(false);
        assertTrue(mContentViewCore.hasSelection());

        setAttachedOnUiThread(true);
        assertWaitForSelectActionBarVisible(true);
        assertTrue(mContentViewCore.hasSelection());
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupNotShownOnLongPressingNonEmptyInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarVisible(true);
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingEmptyInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingNonEmptyInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnTappingOutsideInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "plain_text_2");
        assertWaitForPastePopupStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupClearedOnLongPressingOutsideInput() throws Throwable {
        copyStringToClipboard();
        DOMUtils.longPressNode(this, mContentViewCore, "empty_input_text");
        assertWaitForPastePopupStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text_2");
        assertWaitForPastePopupStatus(false);
    }

    private void assertWaitForSelectActionBarVisible(
            final boolean visible) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return visible == mContentViewCore.isSelectActionBarShowing();
            }
        }));
    }

    private void setVisibileOnUiThread(final boolean show) {
        final ContentViewCore contentViewCore = mContentViewCore;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (show) {
                    contentViewCore.onShow();
                } else {
                    contentViewCore.onHide();
                }
            }
        });
    }

    private void setAttachedOnUiThread(final boolean attached) {
        final ContentViewCore contentViewCore = mContentViewCore;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (attached) {
                    contentViewCore.onAttachedToWindow();
                } else {
                    contentViewCore.onDetachedFromWindow();
                }
            }
        });
    }

    private void requestFocusOnUiThread(final boolean gainFocus) {
        final ContentViewCore contentViewCore = mContentViewCore;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                contentViewCore.onFocusChanged(gainFocus);
            }
        });
    }

    private void copyStringToClipboard() {
        ClipboardManager clipboardManager =
                (ClipboardManager) getActivity().getSystemService(
                        Context.CLIPBOARD_SERVICE);
        ClipData clip = ClipData.newPlainText("test", "Text to copy");
                clipboardManager.setPrimaryClip(clip);
    }

    private void assertWaitForPastePopupStatus(final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == mContentViewCore.getPastePopupForTest().isShowing();
            }
        }));
    }
}
