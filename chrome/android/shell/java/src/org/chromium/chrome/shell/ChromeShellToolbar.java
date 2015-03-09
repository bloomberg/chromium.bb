// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
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

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.widget.SmoothProgressBar;
import org.chromium.chrome.shell.omnibox.SuggestionPopup;
import org.chromium.content.common.ContentSwitches;

/**
 * A Toolbar {@link View} that shows the URL and navigation buttons.
 */
public class ChromeShellToolbar extends LinearLayout {
    private static final long COMPLETED_PROGRESS_TIMEOUT_MS = 200;

    private final Runnable mClearProgressRunnable = new Runnable() {
        @Override
        public void run() {
            mProgressBar.setProgress(0);
        }
    };

    private final Runnable mUpdateProgressRunnable = new Runnable() {
        @Override
        public void run() {
            mProgressBar.setProgress(mProgress);
            if (mLoading) {
                mStopReloadButton.setImageResource(
                        R.drawable.btn_close);
            } else {
                mStopReloadButton.setImageResource(R.drawable.btn_toolbar_reload);
                ApiCompatibilityUtils.postOnAnimationDelayed(ChromeShellToolbar.this,
                        mClearProgressRunnable, COMPLETED_PROGRESS_TIMEOUT_MS);
            }
        }
    };

    private EditText mUrlTextView;
    private SmoothProgressBar mProgressBar;

    private ChromeShellTab mTab;
    private final TabObserver mTabObserver;

    private AppMenuHandler mMenuHandler;
    private AppMenuButtonHelper mAppMenuButtonHelper;

    private TabManager mTabManager;

    private SuggestionPopup mSuggestionPopup;

    private ImageButton mStopReloadButton;
    private ImageButton mAddButton;
    private int mProgress = 0;
    private boolean mLoading = true;
    private boolean mFocus = false;

    /**
     * @param context The Context the view is running in.
     * @param attrs   The attributes of the XML tag that is inflating the view.
     */
    public ChromeShellToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        // When running performance benchmark, we don't want to observe the tab
        // invalidation which would interfere with browser's processing content
        // frame. See crbug.com/394976.
        if (CommandLine.getInstance().hasSwitch(
                ContentSwitches.RUNNING_PERFORMANCE_BENCHMARK)) {
            mTabObserver = new EmptyTabObserver();
        } else {
            mTabObserver = new TabObserverImpl();
        }
    }

    /**
     * The toolbar will visually represent the state of {@code tab}.
     * @param tab The Tab that should be represented.
     */
    public void showTab(ChromeShellTab tab) {
        if (mTab != null) mTab.removeObserver(mTabObserver);

        mTab = tab;

        if (mTab != null) {
            mTab.addObserver(mTabObserver);
            mUrlTextView.setText(mTab.getWebContents().getUrl());
        }
    }

    /**
     * Set the TabManager responsible for activating the tab switcher.
     * @param tabManager The active TabManager.
     */
    public void setTabManager(TabManager tabManager) {
        mTabManager = tabManager;
    }

    private void onUpdateUrl(String url) {
        mUrlTextView.setText(url);
    }

    private void onLoadProgressChanged(int progress) {
        removeCallbacks(mClearProgressRunnable);
        removeCallbacks(mUpdateProgressRunnable);
        mProgress = progress;
        mLoading = progress != 100;
        ApiCompatibilityUtils.postOnAnimation(this, mUpdateProgressRunnable);
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

        mProgressBar = (SmoothProgressBar) findViewById(R.id.progress);
        initializeUrlField();
        initializeTabSwitcherButton();
        initializeMenuButton();
        initializeStopReloadButton();
        initializeAddButton();
    }

    void setMenuHandler(AppMenuHandler menuHandler) {
        mMenuHandler = menuHandler;
        mAppMenuButtonHelper = new AppMenuButtonHelper(mMenuHandler);
    }

    private void initializeUrlField() {
        mUrlTextView = (EditText) findViewById(R.id.url);
        mUrlTextView.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if ((actionId != EditorInfo.IME_ACTION_GO) && (event == null
                        || event.getKeyCode() != KeyEvent.KEYCODE_ENTER
                        || event.getAction() != KeyEvent.ACTION_DOWN)) {
                    return false;
                }

                // This will set |mTab| by calling showTab().
                // TODO(aurimas): Factor out initial tab creation to the activity level.
                Tab tab = mTabManager.openUrl(
                        UrlUtilities.fixupUrl(mUrlTextView.getText().toString()));
                mUrlTextView.clearFocus();
                setKeyboardVisibilityForUrl(false);
                tab.getView().requestFocus();
                return true;
            }
        });
        mUrlTextView.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                setKeyboardVisibilityForUrl(hasFocus);
                mFocus = hasFocus;
                updateToolbarState();
                if (!hasFocus && mTab != null) {
                    mUrlTextView.setText(mTab.getWebContents().getUrl());
                    mSuggestionPopup.dismissPopup();
                }
            }
        });
        mUrlTextView.setOnKeyListener(new OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_BACK) {
                    mUrlTextView.clearFocus();
                    if (mTab != null) {
                        mTab.getView().requestFocus();
                    }
                    return true;
                }
                return false;
            }
        });

        mSuggestionPopup = new SuggestionPopup(getContext(), mUrlTextView, this);
        mUrlTextView.addTextChangedListener(mSuggestionPopup);
    }

    private void initializeTabSwitcherButton() {
        ImageButton tabSwitcherButton = (ImageButton) findViewById(R.id.tab_switcher);
        tabSwitcherButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mTabManager != null) mTabManager.toggleTabSwitcher();
            }
        });
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
            @SuppressLint("ClickableViewAccessibility")
            @Override
            public boolean onTouch(View view, MotionEvent event) {
                return mAppMenuButtonHelper != null && mAppMenuButtonHelper.onTouch(view, event);
            }
        });
    }

    private void initializeStopReloadButton() {
        mStopReloadButton = (ImageButton) findViewById(R.id.stop_reload_button);
        mStopReloadButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mTab == null) return;
                if (mLoading) {
                    mTab.getWebContents().stop();
                } else {
                    mTab.getWebContents().getNavigationController().reload(true);
                }
            }
        });
    }

    private void initializeAddButton() {
        mAddButton = (ImageButton) findViewById(R.id.add_button);
        mAddButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mTabManager.createNewTab();
            }
        });
    }

    /**
     * Shows or hides the add button, the stop/reload button and the URL bar.
     */
    public void updateToolbarState() {
        boolean tabSwitcherState = mTabManager.isTabSwitcherVisible();
        mAddButton.setVisibility(tabSwitcherState ? VISIBLE : GONE);
        mStopReloadButton.setVisibility(tabSwitcherState || mFocus ? GONE : VISIBLE);
        mUrlTextView.setVisibility(tabSwitcherState ? INVISIBLE : VISIBLE);
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
