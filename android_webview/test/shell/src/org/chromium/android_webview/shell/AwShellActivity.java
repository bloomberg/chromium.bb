// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.app.Activity;
import android.content.Intent;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.android_webview.test.NullContentsClient;
import org.chromium.content.browser.LoadUrlParams;

/*
 * This is a lightweight activity for tests that only require WebView functionality.
 */
public class AwShellActivity extends Activity {
    private final static String PREFERENCES_NAME = "AwShellPrefs";
    private final static String INITIAL_URL = "about:blank";
    private AwTestContainerView mAwTestContainerView;
    private EditText mUrlTextView;
    private ImageButton mPrevButton;
    private ImageButton mNextButton;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.testshell_activity);

        mAwTestContainerView = createAwTestContainerView();

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
        mUrlTextView.setText(startupUrl);
    }

    private AwTestContainerView createAwTestContainerView() {
        AwTestContainerView testContainerView = new AwTestContainerView(this);
        AwContentsClient awContentsClient = new NullContentsClient() {
            @Override
            public void onPageStarted(String url) {
                if (mUrlTextView != null) {
                    mUrlTextView.setText(url);
                }
            }
        };

        SharedPreferences sharedPreferences =
            getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE);
        AwBrowserContext browserContext = new AwBrowserContext(sharedPreferences);

        testContainerView.initialize(new AwContents(browserContext, testContainerView,
                testContainerView.getInternalAccessDelegate(),
                awContentsClient, false));
        testContainerView.getContentViewCore().getContentSettings().setJavaScriptEnabled(true);
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
                        event.getKeyCode() != KeyEvent.ACTION_DOWN)) {
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
                    mUrlTextView.setText(mAwTestContainerView.getContentViewCore().getUrl());
                }
            }
        });
    }

    private void initializeNavigationButtons() {
        mPrevButton = (ImageButton) findViewById(R.id.prev);
        mPrevButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mAwTestContainerView.getContentViewCore().canGoBack()) {
                    mAwTestContainerView.getContentViewCore().goBack();
                }
            }
        });

        mNextButton = (ImageButton) findViewById(R.id.next);
        mNextButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mAwTestContainerView.getContentViewCore().canGoForward()) {
                        mAwTestContainerView.getContentViewCore().goForward();
                }
            }
        });
    }
}
