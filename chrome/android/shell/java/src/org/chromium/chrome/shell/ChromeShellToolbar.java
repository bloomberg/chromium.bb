// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.ClipDrawable;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.shell.omnibox.SuggestionPopup;

/**
 * A Toolbar {@link View} that shows the URL and navigation buttons.
 */
public class ChromeShellToolbar extends LinearLayout {
    private static final long COMPLETED_PROGRESS_TIMEOUT_MS = 200;

    private final Runnable mClearProgressRunnable = new Runnable() {
        @Override
        public void run() {
            mProgressDrawable.setLevel(0);
        }
    };

    private EditText mUrlTextView;
    private ClipDrawable mProgressDrawable;

    private ChromeShellTab mTab;
    private final TabObserver mTabObserver = new TabObserverImpl();

    private AppMenuHandler mMenuHandler;
    private AppMenuButtonHelper mAppMenuButtonHelper;

    private SuggestionPopup mSuggestionPopup;

    /**
     * @param context The Context the view is running in.
     * @param attrs   The attributes of the XML tag that is inflating the view.
     */
    public ChromeShellToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * The toolbar will visually represent the state of {@code tab}.
     * @param tab The Tab that should be represented.
     */
    public void showTab(ChromeShellTab tab) {
        if (mTab != null) mTab.removeObserver(mTabObserver);
        mTab = tab;
        mTab.addObserver(mTabObserver);
        mUrlTextView.setText(mTab.getContentViewCore().getUrl());
    }

    private void onUpdateUrl(String url) {
        mUrlTextView.setText(url);
    }

    private void onLoadProgressChanged(int progress) {
        removeCallbacks(mClearProgressRunnable);
        mProgressDrawable.setLevel(100 * progress);
        if (progress == 100) postDelayed(mClearProgressRunnable, COMPLETED_PROGRESS_TIMEOUT_MS);
    }

    /**
     * Closes the suggestion popup.
     */
    public void hideSuggestions() {
        if (mSuggestionPopup != null) mSuggestionPopup.hideSuggestions();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mProgressDrawable = (ClipDrawable) findViewById(R.id.toolbar).getBackground();
        initializeUrlField();
        initializeMenuButton();
    }

    void setMenuHandler(AppMenuHandler menuHandler) {
        mMenuHandler = menuHandler;
        ImageButton menuButton = (ImageButton) findViewById(R.id.menu_button);
        mAppMenuButtonHelper = new AppMenuButtonHelper(menuButton, mMenuHandler);
    }

    private void initializeUrlField() {
        mUrlTextView = (EditText) findViewById(R.id.url);
        mUrlTextView.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if ((actionId != EditorInfo.IME_ACTION_GO) && (event == null ||
                        event.getKeyCode() != KeyEvent.KEYCODE_ENTER ||
                        event.getAction() != KeyEvent.ACTION_DOWN)) {
                    return false;
                }

                mTab.loadUrlWithSanitization(mUrlTextView.getText().toString());
                mUrlTextView.clearFocus();
                setKeyboardVisibilityForUrl(false);
                mTab.getView().requestFocus();
                return true;
            }
        });
        mUrlTextView.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                setKeyboardVisibilityForUrl(hasFocus);
                if (!hasFocus) {
                    mUrlTextView.setText(mTab.getContentViewCore().getUrl());
                    mSuggestionPopup.dismissPopup();
                }
            }
        });

        mSuggestionPopup = new SuggestionPopup(getContext(), mUrlTextView, this);
        mUrlTextView.addTextChangedListener(mSuggestionPopup);
    }

    private void initializeMenuButton() {
        ImageButton menuButton = (ImageButton) findViewById(R.id.menu_button);
        menuButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                if (mMenuHandler != null) mMenuHandler.showAppMenu(view, false, false);
            }
        });
        menuButton.setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent event) {
                return mAppMenuButtonHelper != null && mAppMenuButtonHelper.onTouch(view, event);
            }
        });
    }

    /**
     * @return Current tab that is shown by ChromeShell.
     */
    public ChromeShellTab getCurrentTab() {
        return mTab;
    }

    /**
     * Change the visibility of the software keyboard.
     * @param visible Whether the keyboard should be shown or hidden.
     */
    public void setKeyboardVisibilityForUrl(boolean visible) {
        InputMethodManager imm = (InputMethodManager) getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        if (visible) {
            imm.showSoftInput(mUrlTextView, InputMethodManager.SHOW_IMPLICIT);
        } else {
            imm.hideSoftInputFromWindow(mUrlTextView.getWindowToken(), 0);
        }
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mMenuHandler != null) mMenuHandler.hideAppMenu();
    }

    private class TabObserverImpl extends EmptyTabObserver {
        @Override
        public void onLoadProgressChanged(Tab tab, int progress) {
            if (tab == mTab) ChromeShellToolbar.this.onLoadProgressChanged(progress);
        }

        @Override
        public void onUpdateUrl(Tab tab, String url) {
            if (tab == mTab) ChromeShellToolbar.this.onUpdateUrl(url);
        }
    }
}
