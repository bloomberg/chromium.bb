// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.Context;
import android.graphics.drawable.ClipDrawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.chrome.browser.TabBase;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.content.browser.LoadUrlParams;

/**
 * A Toolbar {@link View} that shows the URL and navigation buttons.
 */
public class TestShellToolbar extends LinearLayout {
    private static final long COMPLETED_PROGRESS_TIMEOUT_MS = 200;

    private Runnable mClearProgressRunnable = new Runnable() {
        @Override
        public void run() {
            mProgressDrawable.setLevel(0);
        }
    };

    private EditText mUrlTextView;
    private ImageButton mPrevButton;
    private ImageButton mNextButton;

    private ClipDrawable mProgressDrawable;

    private TabBase mTab;
    private TabObserver mTabObserver = new TabObserverImpl();

    /**
     * @param context The Context the view is running in.
     * @param attrs   The attributes of the XML tag that is inflating the view.
     */
    public TestShellToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * The toolbar will visually represent the state of {@code tab}.
     * @param tab The TabBase that should be represented.
     */
    public void showTab(TabBase tab) {
        if (mTab != null) mTab.removeObserver(mTabObserver);
        mTab = tab;
        mTab.addObserver(mTabObserver);
        mUrlTextView.setText(mTab.getContentView().getUrl());
    }

    private void onUpdateUrl(String url) {
        mUrlTextView.setText(url);
    }

    private void onLoadProgressChanged(int progress) {
        removeCallbacks(mClearProgressRunnable);
        mProgressDrawable.setLevel((int) (100.0 * progress));
        if (progress == 100) postDelayed(mClearProgressRunnable, COMPLETED_PROGRESS_TIMEOUT_MS);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mProgressDrawable = (ClipDrawable) findViewById(R.id.toolbar).getBackground();
        initializeUrlField();
        initializeNavigationButtons();
    }

    private void initializeUrlField() {
        mUrlTextView = (EditText) findViewById(R.id.url);
        mUrlTextView.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if ((actionId != EditorInfo.IME_ACTION_GO) && (event == null ||
                        event.getKeyCode() != KeyEvent.KEYCODE_ENTER ||
                        event.getKeyCode() != KeyEvent.ACTION_DOWN)) {
                    return false;
                }

                mTab.loadUrlWithSanitization(mUrlTextView.getText().toString());
                mUrlTextView.clearFocus();
                setKeyboardVisibilityForUrl(false);
                mTab.getContentView().requestFocus();
                return true;
            }
        });
        mUrlTextView.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                setKeyboardVisibilityForUrl(hasFocus);
                mNextButton.setVisibility(hasFocus ? GONE : VISIBLE);
                mPrevButton.setVisibility(hasFocus ? GONE : VISIBLE);
                if (!hasFocus) {
                    mUrlTextView.setText(mTab.getContentView().getUrl());
                }
            }
        });
    }

    private void initializeNavigationButtons() {
        mPrevButton = (ImageButton) findViewById(R.id.prev);
        mPrevButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View arg0) {
                if (mTab.getContentView().canGoBack()) mTab.getContentView().goBack();
            }
        });

        mNextButton = (ImageButton) findViewById(R.id.next);
        mNextButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mTab.getContentView().canGoForward()) mTab.getContentView().goForward();
            }
        });
    }

    private void setKeyboardVisibilityForUrl(boolean visible) {
        InputMethodManager imm = (InputMethodManager) getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        if (visible) {
            imm.showSoftInput(mUrlTextView, InputMethodManager.SHOW_IMPLICIT);
        } else {
            imm.hideSoftInputFromWindow(mUrlTextView.getWindowToken(), 0);
        }
    }

    private class TabObserverImpl implements TabObserver {
        @Override
        public void onLoadProgressChanged(TabBase tab, int progress) {
            if (tab == mTab) TestShellToolbar.this.onLoadProgressChanged(progress);
        }

        @Override
        public void onUpdateUrl(TabBase tab, String url) {
            if (tab == mTab) TestShellToolbar.this.onUpdateUrl(url);
        }

        @Override
        public void onCloseTab(TabBase tab) {
        }
    }
}
