// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.content.browser;

import android.content.Context;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.widget.EditText;

import org.chromium.content.browser.input.InputMethodManagerWrapper;
import org.chromium.content_shell.R;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Test that content view core responds to focus changes correctly.
 */
public class ContentViewCoreFocusTest extends ContentShellTestBase {
    private static class TestInputMethodManagerWrapper extends InputMethodManagerWrapper {
        private boolean hidden = false;
        public TestInputMethodManagerWrapper(Context context) {
            super(context);
        }

        @Override
        public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
            hidden = false;
        }

        @Override
        public boolean hideSoftInputFromWindow(IBinder windowToken, int flags,
                ResultReceiver resultReceiver) {
            hidden = true;
            return true;
        }

        @Override
        public boolean isActive(View view) {
            return true;
        }

        public boolean isHidden() {
            return hidden;
        }
    }

    @UiThreadTest
    @RerunWithUpdatedContainerView
    @SmallTest
    public void testHideImeOnLosingFocus() throws Throwable {
        // Test the IME window is hidden from the content view when the content
        // view loses its focus
        final ContentViewCore contentViewCore = getContentViewCore();
        final View view = contentViewCore.getContainerView();
        final TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(getActivity());
        assertTrue(view.requestFocus());

        contentViewCore.setInputMethodManagerWrapperForTest(immw);

        immw.showSoftInput(view, 0, null);
        assertFalse(immw.isHidden());

        final EditText urlBox = (EditText) getActivity().findViewById(R.id.url);

        assertTrue(urlBox.requestFocus());

        // Now another view has taken focus. The original view loses its focus
        // and the input method window should be hidden
        assertTrue(immw.isHidden());
    }
}
