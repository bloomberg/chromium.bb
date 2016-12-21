// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.support.test.filters.SmallTest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.webkit.JavascriptInterface;
import android.widget.EditText;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;

/**
 * Tests for IME (input method editor) on Android WebView.
 */
public class AwImeTest extends AwTestBase {

    private static class TestJavascriptInterface {
        private final CallbackHelper mFocusCallbackHelper = new CallbackHelper();

        @JavascriptInterface
        public void onEditorFocused() {
            mFocusCallbackHelper.notifyCalled();
        }

        public CallbackHelper getFocusCallbackHelper() {
            return mFocusCallbackHelper;
        }
    }

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private EditText mEditText;
    private final TestJavascriptInterface mTestJavascriptInterface = new TestJavascriptInterface();
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

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
                mTestContainerView.getAwContents().addJavascriptInterface(
                        mTestJavascriptInterface, "test");
                // Let's not test against real input method.
                mInputMethodManagerWrapper = new TestInputMethodManagerWrapper(
                        mTestContainerView.getContentViewCore());
                mTestContainerView.getContentViewCore().getImeAdapterForTest()
                        .setInputMethodManagerWrapperForTest(mInputMethodManagerWrapper);
            }
        });
    }

    private void loadContentEditableBody() throws Exception {
        final String mime = "text/html";
        final String htmlDocument = "<html><body contenteditable id='editor'></body></html>";
        final CallbackHelper loadHelper = mContentsClient.getOnPageFinishedHelper();

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
        // View focus may not have been propagated to the renderer process yet. If document is not
        // yet focused, and focusing on an element is an invalid operation. See crbug.com/622151
        // for details.
        executeJavaScriptAndWaitForResult(mTestContainerView.getAwContents(), mContentsClient,
                "function onDocumentFocused() {\n"
                + "  document.getElementById('editor').focus();\n"
                + "  test.onEditorFocused();\n"
                + "}\n"
                + "(function() {\n"
                + "if (document.hasFocus()) {\n"
                + "  onDocumentFocused();"
                + "} else {\n"
                + "  window.addEventListener('focus', onDocumentFocused);\n"
                + "}})();");
        mTestJavascriptInterface.getFocusCallbackHelper().waitForCallback(0);
    }

    private void waitForNonNullInputConnection() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                InputConnection inputConnection = mTestContainerView.getContentViewCore()
                        .getImeAdapterForTest().getInputConnectionForTest();
                return inputConnection != null;
            }
        });
    }

    /**
     * Tests that moving from EditText to WebView keeps the keyboard showing.
     */
    // https://crbug.com/569556
    @SmallTest
    @Feature({"AndroidWebView", "TextInput"})
    public void testPressNextFromEditTextAndType() throws Throwable {
        loadContentEditableBody();
        focusOnEditTextAndShowKeyboard();
        focusOnWebViewAndEnableEditing();
        waitForNonNullInputConnection();
    }
}