// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.os.Handler;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.vr_shell.keyboard.KeyboardEdit;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;

/**
 * Helper class for interfacing with the active {@link InputConnection}.
 */
@JNINamespace("vr")
public class VrInputConnection {
    private static final String TAG = "cr_VrIC";
    private static final boolean DEBUG_LOGS = false;
    private static final int CHARS_AROUND_CURSOR = 100;

    private final long mNativeVrInputConnection;
    private ImeAdapter mImeAdapter;
    private Handler mImeThreadResponseHandler;

    @CalledByNative
    private static VrInputConnection create(long nativeVrInputConnection, WebContents contents) {
        return new VrInputConnection(nativeVrInputConnection, contents);
    }

    private VrInputConnection(long nativeVrInputConnection, WebContents contents) {
        mNativeVrInputConnection = nativeVrInputConnection;
        mImeAdapter = ImeAdapter.fromWebContents(contents);
    }

    @SuppressWarnings("NewApi")
    @CalledByNative
    public void requestTextState() {
        if (DEBUG_LOGS) Log.i(TAG, "requestTextState");
        InputConnection ic = mImeAdapter.getActiveInputConnection();
        assert ic != null;
        if (mImeThreadResponseHandler == null) {
            mImeThreadResponseHandler = new Handler();
        }
        ic.getHandler().post(new Runnable() {
            @Override
            public void run() {
                ic.beginBatchEdit();
                CharSequence before = ic.getTextBeforeCursor(CHARS_AROUND_CURSOR, 0);
                CharSequence selected = ic.getSelectedText(0);
                CharSequence after = ic.getTextAfterCursor(CHARS_AROUND_CURSOR, 0);
                final String textState = (before != null ? before.toString() : "")
                        + (selected != null ? selected.toString() : "")
                        + (after != null ? after.toString() : "");
                ic.endBatchEdit();
                if (DEBUG_LOGS) Log.i(TAG, "text state" + textState);
                // The text state is obtained on the IME thread and the response is sent back to the
                // thread that created this connection.
                mImeThreadResponseHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        nativeUpdateTextState(mNativeVrInputConnection, textState);
                    }
                });
            }
        });
    }

    @SuppressWarnings("NewApi")
    @CalledByNative
    public void onKeyboardEdit(KeyboardEdit[] edits) {
        if (edits.length == 0) return;
        InputConnection ic = mImeAdapter.getActiveInputConnection();
        assert ic != null;
        ic.getHandler().post(new Runnable() {
            @Override
            public void run() {
                for (KeyboardEdit edit : edits) {
                    switch (edit.mType) {
                        case KeyboardEditType.SUBMIT:
                            ic.performEditorAction(EditorInfo.IME_ACTION_GO);
                            break;
                        case KeyboardEditType.COMMIT_TEXT:
                            ic.commitText(edit.mText, edit.mNewCursorPosition);
                            break;
                        case KeyboardEditType.DELETE_TEXT:
                            ic.deleteSurroundingText(-edit.mNewCursorPosition, 0);
                            break;
                        default:
                            assert false;
                    }
                }
            }
        });
    }

    private native void nativeUpdateTextState(long nativeVrInputConnection, String text);
}
