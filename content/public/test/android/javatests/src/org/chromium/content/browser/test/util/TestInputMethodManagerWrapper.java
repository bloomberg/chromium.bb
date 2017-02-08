// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.os.IBinder;
import android.os.ResultReceiver;
import android.util.Pair;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.Log;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.input.InputMethodManagerWrapper;
import org.chromium.content.browser.input.Range;

import java.util.ArrayList;
import java.util.List;

/**
 * Overrides InputMethodManagerWrapper for testing purposes.
 */
@UsedByReflection("ThreadedInputConnectionFactory.java")
public class TestInputMethodManagerWrapper extends InputMethodManagerWrapper {
    private static final String TAG = "cr_Ime";

    private final ContentViewCore mContentViewCore;
    private InputConnection mInputConnection;
    private int mRestartInputCounter;
    private int mShowSoftInputCounter;
    private int mHideSoftInputCounter;
    private final Range mSelection = new Range(-1, -1);
    private final Range mComposition = new Range(-1, -1);
    private boolean mIsShowWithoutHideOutstanding;
    private final List<Pair<Range, Range>> mUpdateSelectionList;
    private int mUpdateCursorAnchorInfoCounter;
    private CursorAnchorInfo mLastCursorAnchorInfo;

    public TestInputMethodManagerWrapper(ContentViewCore contentViewCore) {
        super(null);
        Log.d(TAG, "TestInputMethodManagerWrapper constructor");
        mContentViewCore = contentViewCore;
        mUpdateSelectionList = new ArrayList<>();
    }

    @Override
    public void restartInput(View view) {
        mRestartInputCounter++;
        Log.d(TAG, "restartInput: count [%d]", mRestartInputCounter);
        EditorInfo editorInfo = new EditorInfo();
        mInputConnection = mContentViewCore.onCreateInputConnection(editorInfo);
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        mIsShowWithoutHideOutstanding = true;
        mShowSoftInputCounter++;
        Log.d(TAG, "showSoftInput: count [%d]", mShowSoftInputCounter);
        if (mInputConnection != null) return;
        EditorInfo editorInfo = new EditorInfo();
        mInputConnection = mContentViewCore.onCreateInputConnection(editorInfo);
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
        Log.d(TAG, "updateSelection: [%d %d] [%d %d]", selStart, selEnd, candidatesStart,
                candidatesEnd);
        Pair<Range, Range> newUpdateSelection =
                new Pair<>(new Range(selStart, selEnd), new Range(candidatesStart, candidatesEnd));
        Range lastSelection = null;
        Range lastComposition = null;
        if (!mUpdateSelectionList.isEmpty()) {
            Pair<Range, Range> lastUpdateSelection =
                    mUpdateSelectionList.get(mUpdateSelectionList.size() - 1);
            if (lastUpdateSelection.equals(newUpdateSelection)) return;
            lastSelection = lastUpdateSelection.first;
            lastComposition = lastUpdateSelection.second;
        }
        mUpdateSelectionList.add(new Pair<Range, Range>(
                new Range(selStart, selEnd), new Range(candidatesStart, candidatesEnd)));
        mSelection.set(selStart, selEnd);
        mComposition.set(candidatesStart, candidatesEnd);
        onUpdateSelection(lastSelection, lastComposition, mSelection, mComposition);
    }

    @Override
    public void notifyUserAction() {}

    public final List<Pair<Range, Range>> getUpdateSelectionList() {
        return mUpdateSelectionList;
    }

    public int getRestartInputCounter() {
        return mRestartInputCounter;
    }

    @Override
    public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {
        mUpdateCursorAnchorInfoCounter++;
        mLastCursorAnchorInfo = cursorAnchorInfo;
    }

    public int getShowSoftInputCounter() {
        Log.d(TAG, "getShowSoftInputCounter: %d", mShowSoftInputCounter);
        return mShowSoftInputCounter;
    }

    public int getHideSoftInputCounter() {
        return mHideSoftInputCounter;
    }

    public void reset() {
        Log.d(TAG, "reset");
        mRestartInputCounter = 0;
        mShowSoftInputCounter = 0;
        mHideSoftInputCounter = 0;
        mUpdateSelectionList.clear();
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

    public int getUpdateCursorAnchorInfoCounter() {
        return mUpdateCursorAnchorInfoCounter;
    }

    public void clearLastCursorAnchorInfo() {
        mLastCursorAnchorInfo = null;
    }

    public CursorAnchorInfo getLastCursorAnchorInfo() {
        return mLastCursorAnchorInfo;
    }

    public void onUpdateSelection(Range oldSel, Range oldComp, Range newSel, Range newComp) {}

    public void expectsSelectionOutsideComposition() {}
}
