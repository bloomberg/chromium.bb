// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Message;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

/**
 * A main activity to handle WPT requests.
 *
 * This is currently implemented to support minimum viable implementation such that
 * Multi-window and JavaScript are enabled by default.
 *
 * TODO(crbug.com/994939): It is currently implemented to support only a single child. Although not
 *                         explicitly stated, there may be some WPT tests that require more than one
 *                         children, which is not supported for now.
 */
public class WebPlatformTestsActivity extends Activity {
    private static final String TAG = "WPTActivity";

    /**
     * A callback for testing.
     */
    @VisibleForTesting
    public interface TestCallback {
        /** Called after child layout becomes visible. */
        void onChildLayoutVisible();
        /** Called after child layout becomes invisible. */
        void onChildLayoutInvisible();
    }

    private LayoutInflater mLayoutInflater;
    private RelativeLayout mRootLayout;
    private WebView mWebView;
    private WebView mChildWebView;
    private LinearLayout mChildLayout;
    private TextView mChildTitleText;
    private TestCallback mTestCallback;

    private class MultiWindowWebChromeClient extends WebChromeClient {
        @Override
        public boolean onCreateWindow(
                WebView webView, boolean isDialog, boolean isUserGesture, Message resultMsg) {
            removeAndDestroyChildWebView();
            // Note that WebView is not inflated but created programmatically
            // such that it can be destroyed separately.
            mChildWebView = new WebView(WebPlatformTestsActivity.this);
            WebSettings settings = mChildWebView.getSettings();
            setUpWebSettings(settings);
            settings.setUseWideViewPort(false);
            mChildWebView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView childWebView, String url) {
                    // Once the view has loaded, display its title for debugging.
                    mChildTitleText.setText(childWebView.getTitle());
                }
            });
            mChildWebView.setWebChromeClient(new WebChromeClient() {
                @Override
                public void onCloseWindow(WebView childWebView) {
                    closeChild();
                }
            });
            // Add the new WebView to the layout
            mChildWebView.setLayoutParams(
                    new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.MATCH_PARENT));
            // Tell the transport about the new view
            WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
            transport.setWebView(mChildWebView);
            resultMsg.sendToTarget();

            mChildLayout.addView(mChildWebView);
            // Make the child webview's layout visible
            mChildLayout.setVisibility(View.VISIBLE);
            if (mTestCallback != null) mTestCallback.onChildLayoutVisible();
            return true;
        }

        @Override
        public void onCloseWindow(WebView webView) {
            // Ignore window.close() on the test runner window.
        }
    }

    /** Remove and destroy a child webview if it exists. */
    private void removeAndDestroyChildWebView() {
        if (mChildWebView == null) return;
        ViewGroup parent = (ViewGroup) mChildWebView.getParent();
        if (parent != null) parent.removeView(mChildWebView);
        mChildWebView.destroy();
        mChildWebView = null;
    }

    private String getUrlFromIntent() {
        if (getIntent() == null) return null;
        return getIntent().getDataString();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        WebView.setWebContentsDebuggingEnabled(true);
        mLayoutInflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        setContentView(R.layout.activity_web_platform_tests);
        mRootLayout = findViewById(R.id.rootLayout);
        mWebView = findViewById(R.id.webview);
        mChildLayout = createChildLayout();

        String url = getUrlFromIntent();
        if (url == null) {
            // This is equivalent to Chrome's WPT setup.
            setUpMainWebView("about:blank");
        } else {
            Log.w(TAG,
                    "Handling a non-empty intent. This should only be used for testing. URL: "
                            + url);
            setUpMainWebView(url);
        }
    }

    private LinearLayout createChildLayout() {
        // Provide parent such that MATCH_PARENT layout params can work. Ignore the return value
        // which is mRootLayout.
        mLayoutInflater.inflate(R.layout.activity_web_platform_tests_child, mRootLayout);
        // Choose what has just been added.
        LinearLayout childLayout =
                (LinearLayout) mRootLayout.getChildAt(mRootLayout.getChildCount() - 1);

        mChildTitleText = childLayout.findViewById(R.id.childTitleText);
        Button childCloseButton = childLayout.findViewById(R.id.childCloseButton);
        childCloseButton.setOnClickListener((View v) -> { closeChild(); });
        return childLayout;
    }

    private void setUpWebSettings(WebSettings settings) {
        settings.setJavaScriptEnabled(true);
        // Enable multi-window.
        settings.setJavaScriptCanOpenWindowsAutomatically(true);
        settings.setSupportMultipleWindows(true);
    }

    private void setUpMainWebView(String url) {
        setUpWebSettings(mWebView.getSettings());
        mWebView.setWebChromeClient(new MultiWindowWebChromeClient());
        mWebView.loadUrl(url);
    }

    private void closeChild() {
        mChildTitleText.setText("");
        mChildLayout.setVisibility(View.INVISIBLE);
        removeAndDestroyChildWebView();
        if (mTestCallback != null) mTestCallback.onChildLayoutInvisible();
    }

    @VisibleForTesting
    public void setTestCallback(TestCallback testDelegate) {
        mTestCallback = testDelegate;
    }

    @VisibleForTesting
    public WebView getTestRunnerWebView() {
        return mWebView;
    }
}
