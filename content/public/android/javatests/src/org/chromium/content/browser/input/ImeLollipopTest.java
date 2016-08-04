// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.TargetApi;
import android.os.Build;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Integration tests for text input for Android L (or above) features.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class ImeLollipopTest extends ImeTest {
    @MediumTest
    @Feature({"TextInput"})
    public void testUpdateCursorAnchorInfo() throws Throwable {
        requestCursorUpdates(InputConnection.CURSOR_UPDATE_MONITOR);

        // In "MONITOR" mode, the change should be notified.
        setComposingText("ab", 1);
        waitForUpdateCursorAnchorInfoComposingText("ab");

        CursorAnchorInfo info = mInputMethodManagerWrapper.getLastCursorAnchorInfo();
        assertEquals(0, info.getComposingTextStart());
        assertNotNull(info.getCharacterBounds(0));
        assertNotNull(info.getCharacterBounds(1));
        assertNull(info.getCharacterBounds(2));

        // Should be notified not only once. Further change should be sent, too.
        setComposingText("abcd", 1);
        waitForUpdateCursorAnchorInfoComposingText("abcd");

        info = mInputMethodManagerWrapper.getLastCursorAnchorInfo();
        assertEquals(0, info.getComposingTextStart());
        assertNotNull(info.getCharacterBounds(0));
        assertNotNull(info.getCharacterBounds(1));
        assertNotNull(info.getCharacterBounds(2));
        assertNotNull(info.getCharacterBounds(3));
        assertNull(info.getCharacterBounds(4));

        // In "IMMEDIATE" mode, even when there's no change, we should be notified at least once.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mInputMethodManagerWrapper.clearLastCursorAnchorInfo();
            }
        });
        requestCursorUpdates(InputConnection.CURSOR_UPDATE_IMMEDIATE);
        waitForUpdateCursorAnchorInfoComposingText("abcd");
    }

    private void requestCursorUpdates(final int cursorUpdateMode) {
        final InputConnection connection = mConnection;
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.requestCursorUpdates(cursorUpdateMode);
            }
        });
    }

    private void waitForUpdateCursorAnchorInfoComposingText(final String expected)
            throws InterruptedException {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                CursorAnchorInfo info = mInputMethodManagerWrapper.getLastCursorAnchorInfo();
                if (info != null && info.getComposingText() == null) {
                    updateFailureReason("info.getCompositingText() returned null");
                    return false;
                }

                String actual = (info == null ? "" : info.getComposingText().toString());
                updateFailureReason("Expected: {" + expected + "}, Actual: {" + actual + "}");
                return expected.equals(actual);
            }
        });
    }
}
