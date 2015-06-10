// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.os.IBinder;
import android.os.ResultReceiver;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.input.InputMethodManagerWrapper;

/**
 * Overrides InputMethodManagerWrapper for testing purposes.
 */
public class TestInputMethodManagerWrapper extends InputMethodManagerWrapper {
    /**
     * A simple class to set start and end in int type.
     */
    public static class Range {
        private int mStart;
        private int mEnd;

        public Range(int start, int end) {
            mStart = start;
            mEnd = end;
        }

        public void set(int start, int end) {
            mStart = start;
            mEnd = end;
        }

        public int start() {
            return mStart;
        }

        public int end() {
            return mEnd;
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof Range)) return false;
            if (o == this) return true;
            Range r = (Range) o;
            return mStart == r.mStart && mEnd == r.mEnd;
        }

        @Override
        public int hashCode() {
            final int prime = 31;
            return prime * mStart + mEnd;
        }

        @Override
        public String toString() {
            return "[ " + mStart + ", " + mEnd + " ]";
        }
    }

    private final ContentViewCore mContentViewCore;
    private InputConnection mInputConnection;
    private int mShowSoftInputCounter = 0;
    private int mUpdateSelectionCounter = 0;
    private EditorInfo mEditorInfo;
    private final Range mSelection = new Range(0, 0);
    private final Range mComposition = new Range(-1, -1);

    public TestInputMethodManagerWrapper(ContentViewCore contentViewCore) {
        super(null);
        mContentViewCore = contentViewCore;
    }

    @Override
    public void restartInput(View view) {
        mEditorInfo = new EditorInfo();
        mInputConnection = mContentViewCore.onCreateInputConnection(mEditorInfo);
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        mShowSoftInputCounter++;
        if (mInputConnection != null) return;
        mEditorInfo = new EditorInfo();
        mInputConnection = mContentViewCore.onCreateInputConnection(mEditorInfo);
    }

    @Override
    public boolean isActive(View view) {
        if (mInputConnection == null) return false;
        return true;
    }

    @Override
    public boolean hideSoftInputFromWindow(IBinder windowToken, int flags,
            ResultReceiver resultReceiver) {
        boolean retVal = mInputConnection == null;
        mInputConnection = null;
        return retVal;
    }

    @Override
    public void updateSelection(View view, int selStart, int selEnd,
            int candidatesStart, int candidatesEnd) {
        mUpdateSelectionCounter++;
        mSelection.set(selStart, selEnd);
        mComposition.set(candidatesStart, candidatesEnd);
    }

    public int getShowSoftInputCounter() {
        return mShowSoftInputCounter;
    }

    public int getUpdateSelectionCounter() {
        return mUpdateSelectionCounter;
    }

    public EditorInfo getEditorInfo() {
        return mEditorInfo;
    }

    public Range getSelection() {
        return mSelection;
    }

    public Range getComposition() {
        return mComposition;
    }
}