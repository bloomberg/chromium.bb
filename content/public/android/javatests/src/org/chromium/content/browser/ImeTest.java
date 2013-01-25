// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.MediumTest;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.content_shell.ContentShellTestBase;

public class ImeTest extends ContentShellTestBase {

    private static final String DATA_URL =
            "data:text/html;utf-8,<html><body>" +
            "<input id=\"input_text\" type=\"text\" />" +
            "</body></html>";

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        final ContentView view = getActivity().getActiveContentView();
        final TestCallbackHelperContainer viewClient =
                new TestCallbackHelperContainer(view);
        DOMUtils.clickNode(this, view, viewClient, "input_text");
        assertWaitForKeyboardStatus(true);

        getAdapterInputConnection().setComposingText("hello", 5);
        getAdapterInputConnection().performEditorAction(EditorInfo.IME_ACTION_GO);

        // Since hiding the keyboard is an asynchronous task, it might take an arbitrary amount
        // of time. settleDownUI will wait for one second and hopefully allowing the keyboard to
        // get to it's final state.
        UiUtils.settleDownUI(getInstrumentation());
        assertWaitForKeyboardStatus(false);
    }

    private void assertWaitForKeyboardStatus(final boolean show) throws Throwable {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == getImeAdapter().mIsShowWithoutHideOutstanding;
            }
        }));
    }

    private ImeAdapter getImeAdapter() {
        return getContentViewCore().getImeAdapterForTest();
    }

    private ImeAdapter.AdapterInputConnection getAdapterInputConnection() {
        return getContentViewCore().getInputConnectionForTest();
    }
}
