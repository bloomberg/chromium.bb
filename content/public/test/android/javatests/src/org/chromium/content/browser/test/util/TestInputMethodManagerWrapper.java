// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.os.IBinder;
import android.os.ResultReceiver;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.Log;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.input.InputMethodManagerWrapper;

/**
 * Overrides InputMethodManagerWrapper for testing purposes.
 */
public class TestInputMethodManagerWrapper extends InputMethodManagerWrapper {
    private static final String TAG = "cr_Ime";

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
    private int mRestartInputCounter;
    private int mShowSoftInputCounter;
    private int mHideSoftInputCounter;
    private int mUpdateSelectionCounter;
    private EditorInfo mEditorInfo;
    private final Range mSelection = new Range(0, 0);
    private final Range mComposition = new Range(-1, -1);
    private boolean mIsShowWithoutHideOutstanding;

    public TestInputMethodManagerWrapper(ContentViewCore contentViewCore) {
        super(null);
        Log.d(TAG, "TestInputMethodManagerWrapper constructor");
        mContentViewCore = contentViewCore;
    }

    @Override
    public void restartInput(View view) {
        mRestartInputCounter++;
        Log.d(TAG, "restartInput: count [%d]", mRestartInputCounter);
        mEditorInfo = new EditorInfo();
        mInputConnection = mContentViewCore.onCreateInputConnection(mEditorInfo);
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        mIsShowWithoutHideOutstanding = true;
        mShowSoftInputCounter++;
        Log.d(TAG, "showSoftInput: count [%d]", mShowSoftInputCounter);
        if (mInputConnection != null) return;
        mEditorInfo = new EditorInfo();
        mInputConnection = mContentViewCore.onCreateInputConnection(mEditorInfo);
    }

    @Override
    public boolean isActive(View view) {
        boolean result = mInputConnection != null;
        Log.d(TAG, "isActive: returns [%b]", result);
        return result;
    }

    @Override
    public boolean hideSoftInputFromWindow(IBinder windowToken, int flags,
            ResultReceiver resultReceiver) {
        mIsShowWithoutHideOutstanding = false;
        mHideSoftInputCounter++;
        Log.d(TAG, "hideSoftInputFromWindow: count [%d]", mHideSoftInputCounter);
        boolean retVal = mInputConnection == null;
        mInputConnection = null;
        return retVal;
    }

    @Override
    public void updateSelection(View view, int selStart, int selEnd,
            int candidatesStart, int candidatesEnd) {
        Log.d(TAG, "updateSelection");
        mUpdateSelectionCounter++;
        mSelection.set(selStart, selEnd);
        mComposition.set(candidatesStart, candidatesEnd);
    }

    public int getRestartInputCounter() {
        return mRestartInputCounter;
    }

    public int getShowSoftInputCounter() {
        Log.d(TAG, "getShowSoftInputCounter: %d", mShowSoftInputCounter);
        return mShowSoftInputCounter;
    }

    public int getHideSoftInputCounter() {
        return mHideSoftInputCounter;
    }

    public int getUpdateSelectionCounter() {
        return mUpdateSelectionCounter;
    }

    public void resetCounters() {
        Log.d(TAG, "resetCounters");
        mRestartInputCounter = 0;
        mShowSoftInputCounter = 0;
        mHideSoftInputCounter = 0;
        mUpdateSelectionCounter = 0;
    }

    public EditorInfo getEditorInfo() {
        return mEditorInfo;
    }

    public InputConnection getInputConnection() {
        return mInputConnection;
    }

    public Range getSelection() {
        return mSelection;
    }

    public Range getComposition() {
        return mComposition;
    }

    public boolean isShowWithoutHideOutstanding() {
        return mIsShowWithoutHideOutstanding;
    }
}