// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests for IME (input method editor) on Android WebView.
 */
public class AwImeTest extends AwTestBase {

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private EditText mEditText;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Use detached container view to avoid focus request.
                mTestContainerView = createDetachedAwTestContainerView(mContentsClient);
                mEditText = new EditText(getActivity());
                getActivity().addView(mEditText);
                getActivity().addView(mTestContainerView);
            }
        });

        final CallbackHelper loadHelper = mContentsClient.getOnPageFinishedHelper();

        final String mime = "text/html";
        final String htmlDocument = "<html><body contenteditable id='editor'></body></html>";

        loadDataSync(mTestContainerView.getAwContents(), loadHelper, htmlDocument, mime, false);
    }

    private void focusOnEditTextAndShowKeyboard() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mEditText.requestFocus();
                InputMethodManager imm = (InputMethodManager) getActivity().getSystemService(
                        Context.INPUT_METHOD_SERVICE);
                imm.showSoftInput(mEditText, 0);
            }
        });
    }

    private void focusOnWebViewAndEnableEditing() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTestContainerView.requestFocus();
            }
        });

        enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
        // This is the usual pattern for email client using webview:
        // we want the cursor to be there even without tapping the editor.
        executeJavaScriptAndWaitForResult(mTestContainerView.getAwContents(), mContentsClient,
                "(function() {"
                + "var sel = window.getSelection();"
                + "var range = document.createRange();"
                + "var editor = document.getElementById('editor');"
                + "range.setStart(editor, 0);"
                + "range.setEnd(editor, 0);"
                + "range.collapse(false);"
                + "sel.removeAllRanges();"
                + "sel.addRange(range);"
                + "})();");
    }

    private void waitForNonNullInputConnection() throws InterruptedException {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                InputConnection inputConnection = mTestContainerView.getContentViewCore()
                        .getImeAdapterForTest().getInputConnectionForTest();
                return inputConnection != null;
            }
        });
    }

    // https://crbug.com/569556
    @SmallTest
    @Feature({"AndroidWebView", "TextInput"})
    public void testPressNextFromEditTextAndType() throws Throwable {
        focusOnEditTextAndShowKeyboard();
        focusOnWebViewAndEnableEditing();
        waitForNonNullInputConnection();
    }
}