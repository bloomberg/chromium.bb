// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.content.browser;

import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;

import org.chromium.content.browser.input.ImeAdapter;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Tests that ContentViewCore responds to DPAD key events correctly
 */
public class ContentViewCoreDpadTest extends ContentShellTestBase {
    TestContentViewCore mContentViewCore;
    private static class TestImeAdapter extends ImeAdapter {
        public TestImeAdapter() {
            super(null, null);
        }

        @Override
        public boolean dispatchKeyEvent(KeyEvent event) {
            return true;
        }
    }

    private static class TestContentViewCore extends ContentViewCore {
        private boolean mIsShowImeIfNeededCalled = false;
        public TestContentViewCore(Context context) {
            super(context);
        }

        public boolean isShowImeIfNeededCalled() {
            return mIsShowImeIfNeededCalled;
        }

        @Override
        public void showImeIfNeeded() {
            mIsShowImeIfNeededCalled = true;
        }
    }

    public void setUp() throws Exception {
        super.setUp();
        mContentViewCore = new TestContentViewCore(getActivity());
        mContentViewCore.setImeAdapterForTest(new TestImeAdapter());
    }

    @SmallTest
    // Test KEYCODE_DPAD_CENTER keyevent will bring up the IME, if needed
    public void testDpadCenterBringsUpIme() throws Exception {
        KeyEvent event = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER);

        mContentViewCore.dispatchKeyEvent(event);

        // Just assert showImeIfNeeded is called. Software keyboard may not show if a QWERTY
        // keyboard is connected or if the focused object is not editable.
        assertTrue(mContentViewCore.isShowImeIfNeededCalled());
    }
}
