// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ImeAdapter.AdapterInputConnection;
import org.chromium.content.browser.ImeAdapter.AdapterInputConnectionFactory;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.content_shell.ContentShellTestBase;

public class ImeTest extends ContentShellTestBase {

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><body>" +
            "<form action=\"about:blank\">" +
            "<input id=\"input_text\" type=\"text\" />" +
            "</form></body></html>");

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        getContentViewCore().setAdapterInputConnectionFactory(
                new TestAdapterInputConnectionFactory());

        final ContentView view = getActivity().getActiveContentView();
        final TestCallbackHelperContainer viewClient =
                new TestCallbackHelperContainer(view);
        DOMUtils.clickNode(this, view, viewClient, "input_text");
        assertWaitForKeyboardStatus(true);

        TestAdapterInputConnection connection =
                (TestAdapterInputConnection) getAdapterInputConnection();
        ImeAdapter adapter = getImeAdapter();

        assertWaitForSetEditableCallback(1, connection);
        assertEquals("", connection.mText);
        assertEquals(0, connection.mSelectionStart);
        assertEquals(0, connection.mSelectionEnd);
        assertEquals(-1, connection.mCompositionStart);
        assertEquals(-1, connection.mCompositionEnd);

        adapter.checkCompositionQueueAndCallNative("hello", 1, false);
        assertWaitForSetEditableCallback(2, connection);
        assertEquals("hello", connection.mText);
        assertEquals(5, connection.mSelectionStart);
        assertEquals(5, connection.mSelectionEnd);
        assertEquals(0, connection.mCompositionStart);
        assertEquals(5, connection.mCompositionEnd);

        performGo(getAdapterInputConnection(), viewClient);

        assertWaitForSetEditableCallback(3, connection);
        assertEquals("", connection.mText);
        assertEquals(0, connection.mSelectionStart);
        assertEquals(0, connection.mSelectionEnd);
        assertEquals(-1, connection.mCompositionStart);
        assertEquals(-1, connection.mCompositionEnd);
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testGetTextUpdatesAfterEnteringText() throws Throwable {
        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        getContentViewCore().setAdapterInputConnectionFactory(
                new TestAdapterInputConnectionFactory());

        final ContentView view = getActivity().getActiveContentView();
        final TestCallbackHelperContainer viewClient =
                new TestCallbackHelperContainer(view);
        DOMUtils.clickNode(this, view, viewClient, "input_text");
        assertWaitForKeyboardStatus(true);

        TestAdapterInputConnection connection =
                (TestAdapterInputConnection) getAdapterInputConnection();
        ImeAdapter adapter = getImeAdapter();

        assertWaitForSetEditableCallback(1, connection);
        assertEquals("", connection.mText);
        assertEquals(0, connection.mSelectionStart);
        assertEquals(0, connection.mSelectionEnd);
        assertEquals(-1, connection.mCompositionStart);
        assertEquals(-1, connection.mCompositionEnd);

        adapter.checkCompositionQueueAndCallNative("h", 1, false);
        assertWaitForSetEditableCallback(2, connection);
        assertEquals("h", connection.mText);
        assertEquals(1, connection.mSelectionStart);
        assertEquals(1, connection.mSelectionEnd);
        assertEquals(0, connection.mCompositionStart);
        assertEquals(1, connection.mCompositionEnd);

        adapter.checkCompositionQueueAndCallNative("he", 1, false);
        assertWaitForSetEditableCallback(3, connection);
        assertEquals("he", connection.mText);
        assertEquals(2, connection.mSelectionStart);
        assertEquals(2, connection.mSelectionEnd);
        assertEquals(0, connection.mCompositionStart);
        assertEquals(2, connection.mCompositionEnd);

        adapter.checkCompositionQueueAndCallNative("hel", 1, false);
        assertWaitForSetEditableCallback(4, connection);
        assertEquals("hel", connection.mText);
        assertEquals(3, connection.mSelectionStart);
        assertEquals(3, connection.mSelectionEnd);
        assertEquals(0, connection.mCompositionStart);
        assertEquals(3, connection.mCompositionEnd);

        adapter.checkCompositionQueueAndCallNative("hel", 1, true);
        assertWaitForSetEditableCallback(5, connection);
        assertEquals("hel", connection.mText);
        assertEquals(3, connection.mSelectionStart);
        assertEquals(3, connection.mSelectionEnd);
        assertEquals(-1, connection.mCompositionStart);
        assertEquals(-1, connection.mCompositionEnd);
    }

    private void performGo(final AdapterInputConnection inputConnection,
            TestCallbackHelperContainer testCallbackHelperContainer) throws Throwable {
        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        inputConnection.performEditorAction(EditorInfo.IME_ACTION_GO);
                    }
                });
    }

    private void assertWaitForKeyboardStatus(final boolean show) throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == getImeAdapter().mIsShowWithoutHideOutstanding;
            }
        }));
    }

    private void assertWaitForSetEditableCallback(final int callbackNumber,
            final TestAdapterInputConnection connection) throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return callbackNumber == connection.mSetEditableTextCallCounter;
            }
        }));
    }

    private ImeAdapter getImeAdapter() {
        return getContentViewCore().getImeAdapterForTest();
    }

    private ImeAdapter.AdapterInputConnection getAdapterInputConnection() {
        return getContentViewCore().getInputConnectionForTest();
    }

    private static class TestAdapterInputConnectionFactory extends
            ImeAdapter.AdapterInputConnectionFactory {
        @Override
        public AdapterInputConnection get(View view, ImeAdapter imeAdapter,
                EditorInfo outAttrs) {
            return new TestAdapterInputConnection(view, imeAdapter, outAttrs);
        }
    }

    private static class TestAdapterInputConnection extends ImeAdapter.AdapterInputConnection {
        private int mSetEditableTextCallCounter;
        private String mText;
        private int mSelectionStart;
        private int mSelectionEnd;
        private int mCompositionStart;
        private int mCompositionEnd;

        public TestAdapterInputConnection(View view, ImeAdapter imeAdapter, EditorInfo outAttrs) {
            super(view, imeAdapter, outAttrs);
            mSetEditableTextCallCounter = 0;
        }

        @Override
        public void setEditableText(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            mText = text;
            mSelectionStart = selectionStart;
            mSelectionEnd = selectionEnd;
            mCompositionStart = compositionStart;
            mCompositionEnd = compositionEnd;
            mSetEditableTextCallCounter++;
        }
    }
}
