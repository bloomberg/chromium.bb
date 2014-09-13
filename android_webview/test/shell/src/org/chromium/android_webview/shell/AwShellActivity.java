// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.webkit.WebChromeClient;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwDevToolsServer;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.android_webview.test.NullContentsClient;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;

/**
 * This is a lightweight activity for tests that only require WebView functionality.
 */
public class AwShellActivity extends Activity {
    private static final String PREFERENCES_NAME = "AwShellPrefs";
    private static final String INITIAL_URL = "about:blank";
    private AwBrowserContext mBrowserContext;
    private AwDevToolsServer mDevToolsServer;
    private AwTestContainerView mAwTestContainerView;
    private WebContents mWebContents;
    private NavigationController mNavigationController;
    private EditText mUrlTextView;
    private ImageButton mPrevButton;
    private ImageButton mNextButton;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.testshell_activity);

        mAwTestContainerView = createAwTestContainerView();

        mWebContents = mAwTestContainerView.getContentViewCore().getWebContents();
        mNavigationController = mWebContents.getNavigationController();
        LinearLayout contentContainer = (LinearLayout) findViewById(R.id.content_container);
        mAwTestContainerView.setLayoutParams(new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT, 1f));
        contentContainer.addView(mAwTestContainerView);
        mAwTestContainerView.requestFocus();

        initializeUrlField();
        initializeNavigationButtons();

        String startupUrl = getUrlFromIntent(getIntent());
        if (TextUtils.isEmpty(startupUrl)) {
            startupUrl = INITIAL_URL;
        }

        mAwTestContainerView.getAwContents().loadUrl(new LoadUrlParams(startupUrl));
        AwContents.setShouldDownloadFavicons();
        mUrlTextView.setText(startupUrl);
    }

    private AwTestContainerView createAwTestContainerView() {
        AwBrowserProcess.start(this);
        AwTestContainerView testContainerView = new AwTestContainerView(this, true);
        AwContentsClient awContentsClient = new NullContentsClient() {
            private View mCustomView;

            @Override
            public void onPageStarted(String url) {
                if (mUrlTextView != null) {
                    mUrlTextView.setText(url);
                }
            }

            @Override
            public void onShowCustomView(View view, WebChromeClient.CustomViewCallback callback) {
                getWindow().setFlags(
                        WindowManager.LayoutParams.FLAG_FULLSCREEN,
                        WindowManager.LayoutParams.FLAG_FULLSCREEN);

                getWindow().addContentView(view,
                        new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                Gravity.CENTER));
                mCustomView = view;
            }

            @Override
            public void onHideCustomView() {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                FrameLayout decorView = (FrameLayout) getWindow().getDecorView();
                decorView.removeView(mCustomView);
                mCustomView = null;
            }

            @Override
            public boolean shouldOverrideKeyEvent(KeyEvent event) {
                if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
                    return true;
                }
                return false;
            }
        };

        SharedPreferences sharedPreferences =
            getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE);
        if (mBrowserContext == null) {
            mBrowserContext = new AwBrowserContext(sharedPreferences);
        }
        final AwSettings awSettings = new AwSettings(this /*context*/,
                false /*isAccessFromFileURLsGrantedByDefault*/, true /*supportsLegacyQuirks*/);
        // Required for WebGL conformance tests.
        awSettings.setMediaPlaybackRequiresUserGesture(false);
        testContainerView.initialize(new AwContents(mBrowserContext, testContainerView,
                testContainerView.getContext(), testContainerView.getInternalAccessDelegate(),
                testContainerView.getNativeGLDelegate(), awContentsClient, awSettings));
        testContainerView.getAwContents().getSettings().setJavaScriptEnabled(true);
        if (mDevToolsServer == null) {
            mDevToolsServer = new AwDevToolsServer();
            mDevToolsServer.setRemoteDebuggingEnabled(true);
        }
        return testContainerView;
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    private void setKeyboardVisibilityForUrl(boolean visible) {
        InputMethodManager imm = (InputMethodManager) getSystemService(
                Context.INPUT_METHOD_SERVICE);
        if (visible) {
            imm.showSoftInput(mUrlTextView, InputMethodManager.SHOW_IMPLICIT);
        } else {
            imm.hideSoftInputFromWindow(mUrlTextView.getWindowToken(), 0);
        }
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

                mAwTestContainerView.getAwContents().loadUrl(
                        new LoadUrlParams(mUrlTextView.getText().toString()));
                mUrlTextView.clearFocus();
                setKeyboardVisibilityForUrl(false);
                mAwTestContainerView.requestFocus();
                return true;
            }
        });
        mUrlTextView.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                setKeyboardVisibilityForUrl(hasFocus);
                mNextButton.setVisibility(hasFocus ? View.GONE : View.VISIBLE);
                mPrevButton.setVisibility(hasFocus ? View.GONE : View.VISIBLE);
                if (!hasFocus) {
                    mUrlTextView.setText(mWebContents.getUrl());
                }
            }
        });
    }

    private void initializeNavigationButtons() {
        mPrevButton = (ImageButton) findViewById(R.id.prev);
        mPrevButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mNavigationController.canGoBack()) {
                    mNavigationController.goBack();
                }
            }
        });

        mNextButton = (ImageButton) findViewById(R.id.next);
        mNextButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mNavigationController.canGoForward()) {
                    mNavigationController.goForward();
                }
            }
        });
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            if (mNavigationController.canGoBack()) {
                mNavigationController.goBack();
                return true;
            }
        }

        return super.onKeyUp(keyCode, event);
    }
}
