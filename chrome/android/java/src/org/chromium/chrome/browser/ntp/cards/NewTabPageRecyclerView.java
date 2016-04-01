// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * Simple wrapper on top of a RecyclerView that will acquire focus when tapped.  Ensures the
 * New Tab page receives focus when clicked.
 */
public class NewTabPageRecyclerView extends RecyclerView {
    /**
     * Listener for scroll changes.
     */
    public interface OnScrollListener {
        /**
         * Triggered when the scroll changes.  See ScrollView#onScrollChanged for more
         * details.
         */
        void onScrollChanged(int l, int t, int oldl, int oldt);
    }

    private GestureDetector mGestureDetector;
    private OnScrollListener mOnScrollListener;

    /**
     * Constructor needed to inflate from XML.
     */
    public NewTabPageRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mGestureDetector =
                new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onSingleTapUp(MotionEvent e) {
                        boolean retVal = super.onSingleTapUp(e);
                        requestFocus();
                        return retVal;
                    }
                });
        setLayoutManager(new LinearLayoutManager(getContext()));
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        mGestureDetector.onTouchEvent(ev);
        return super.onInterceptTouchEvent(ev);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        // Action down would already have been handled in onInterceptTouchEvent
        if (ev.getActionMasked() != MotionEvent.ACTION_DOWN) {
            mGestureDetector.onTouchEvent(ev);
        }
        return super.onTouchEvent(ev);
    }

    /**
     * Sets the listener to be notified of scroll changes.
     * @param listener The listener to be updated on scroll changes.
     */
    public void setOnScrollListener(OnScrollListener listener) {
        mOnScrollListener = listener;
    }

    @Override
    protected void onScrollChanged(int l, int t, int oldl, int oldt) {
        super.onScrollChanged(l, t, oldl, oldt);
        if (mOnScrollListener != null) mOnScrollListener.onScrollChanged(l, t, oldl, oldt);
    }

    @Override
    public void focusableViewAvailable(View v) {
        // To avoid odd jumps during NTP animation transitions, we do not attempt to give focus
        // to child views if this scroll view already has focus.
        if (hasFocus()) return;
        super.focusableViewAvailable(v);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // Fixes lanscape transitions when unfocusing the URL bar: crbug.com/288546
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return super.onCreateInputConnection(outAttrs);
    }
}
