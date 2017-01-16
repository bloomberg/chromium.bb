// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Handler;
import android.view.KeyCharacterMap;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.concurrent.Callable;

/**
 * Unit tests for {@ThreadedInputConnection}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ThreadedInputConnectionTest {
    @Mock ImeAdapter mImeAdapter;

    ThreadedInputConnection mConnection;
    InOrder mInOrder;
    View mView;
    Context mContext;
    boolean mRunningOnUiThread;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mImeAdapter = Mockito.mock(ImeAdapter.class);
        mInOrder = inOrder(mImeAdapter);

        // Mocks required to create a ThreadedInputConnection object
        mView = Mockito.mock(View.class);
        mContext = Mockito.mock(Context.class);
        when(mView.getContext()).thenReturn(mContext);
        when(mContext.getSystemService(Context.INPUT_METHOD_SERVICE)).thenReturn(Mockito.mock(
                InputMethodManager.class));
        // Let's create Handler for test thread and pretend that it is running on IME thread.
        mConnection = new ThreadedInputConnection(mView, mImeAdapter, new Handler()) {
            @Override
            protected boolean runningOnUiThread() {
                return mRunningOnUiThread;
            }
        };
    }

    @Test
    @Feature({"TextInput"})
    public void testComposeGetTextFinishGetText() {
        // IME app calls setComposingText().
        mConnection.setComposingText("hello", 1);
        mInOrder.verify(mImeAdapter).sendCompositionToNative("hello", 1, false, 0);

        // Renderer updates states asynchronously.
        mConnection.updateStateOnUiThread("hello", 5, 5, 0, 5, true, false);
        mInOrder.verify(mImeAdapter).updateSelection(5, 5, 0, 5);
        assertEquals(0, mConnection.getQueueForTest().size());

        // Prepare to call requestTextInputStateUpdate.
        mConnection.updateStateOnUiThread("hello", 5, 5, 0, 5, true, true);
        assertEquals(1, mConnection.getQueueForTest().size());
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // IME app calls getTextBeforeCursor().
        assertEquals("hello", mConnection.getTextBeforeCursor(20, 0));

        // IME app calls finishComposingText().
        mConnection.finishComposingText();
        mInOrder.verify(mImeAdapter).finishComposingText();
        mConnection.updateStateOnUiThread("hello", 5, 5, -1, -1, true, false);
        mInOrder.verify(mImeAdapter).updateSelection(5, 5, -1, -1);

        // Prepare to call requestTextInputStateUpdate.
        mConnection.updateStateOnUiThread("hello", 5, 5, -1, -1, true, true);
        assertEquals(1, mConnection.getQueueForTest().size());
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // IME app calls getTextBeforeCursor().
        assertEquals("hello", mConnection.getTextBeforeCursor(20, 0));

        assertEquals(0, mConnection.getQueueForTest().size());
    }

    @Test
    @Feature({"TextInput"})
    public void testPressingDeadKey() {
        // On default keyboard "Alt+i" produces a dead key '\u0302'.
        mConnection.setCombiningAccentOnUiThread(0x0302);
        mConnection.updateComposingText("\u0302", 1, true);
        mInOrder.verify(mImeAdapter)
                .sendCompositionToNative(
                        "\u0302", 1, false, 0x0302 | KeyCharacterMap.COMBINING_ACCENT);
    }

    @Test
    @Feature({"TextInput"})
    public void testRenderChangeUpdatesSelection() {
        // User moves the cursor.
        mConnection.updateStateOnUiThread("hello", 4, 4, -1, -1, true, false);
        mInOrder.verify(mImeAdapter).updateSelection(4, 4, -1, -1);
        assertEquals(0, mConnection.getQueueForTest().size());
    }

    @Test
    @Feature({"TextInput"})
    public void testBatchEdit() {
        // IME app calls beginBatchEdit().
        assertTrue(mConnection.beginBatchEdit());
        // Type hello real fast.
        mConnection.commitText("hello", 1);
        mInOrder.verify(mImeAdapter).sendCompositionToNative("hello", 1, true, 0);

        // Renderer updates states asynchronously.
        mConnection.updateStateOnUiThread("hello", 5, 5, -1, -1, true, false);
        mInOrder.verify(mImeAdapter, never()).updateSelection(5, 5, -1, -1);
        assertEquals(0, mConnection.getQueueForTest().size());

        {
            // Nest another batch edit.
            assertTrue(mConnection.beginBatchEdit());
            // Move the cursor to the left.
            mConnection.setSelection(4, 4);
            assertTrue(mConnection.endBatchEdit());
        }
        // We still have one outer batch edit, so should not update selection yet.
        mInOrder.verify(mImeAdapter, never()).updateSelection(4, 4, -1, -1);

        // Prepare to call requestTextInputStateUpdate.
        mConnection.updateStateOnUiThread("hello", 4, 4, -1, -1, true, true);
        assertEquals(1, mConnection.getQueueForTest().size());
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // IME app calls endBatchEdit().
        assertFalse(mConnection.endBatchEdit());
        // Batch edit is finished, now update selection.
        mInOrder.verify(mImeAdapter).updateSelection(4, 4, -1, -1);
        assertEquals(0, mConnection.getQueueForTest().size());
    }

    @Test
    @Feature({"TextInput"})
    @Ignore("crbug/632792")
    public void testFailToRequestToRenderer() {
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(false);
        // Should not hang here. Return null to indicate failure.
        assertNull(null, mConnection.getTextBeforeCursor(10, 0));
    }

    @Test
    @Feature({"TextInput"})
    @Ignore("crbug/632792")
    public void testRendererCannotUpdateState() {
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);
        // We found that renderer cannot update state, e.g., due to a crash.
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    // TODO(changwan): find a way to avoid this.
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                    fail();
                }
                mConnection.unblockOnUiThread();
            }
        });
        // Should not hang here. Return null to indicate failure.
        assertEquals(null, mConnection.getTextBeforeCursor(10, 0));
    }

    // crbug.com/643477
    @Test
    @Feature({"TextInput"})
    public void testUiThreadAccess() {
        assertTrue(mConnection.commitText("hello", 1));
        mRunningOnUiThread = true;
        // Depending on the timing, the result may not be up-to-date.
        assertNotEquals("hello",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return mConnection.getTextBeforeCursor(10, 0);
                    }
                }));
        // Or it could be.
        mConnection.updateStateOnUiThread("hello", 5, 5, -1, -1, true, false);
        assertEquals("hello",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return mConnection.getTextBeforeCursor(10, 0);
                    }
                }));

        mRunningOnUiThread = false;
    }
}
