// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.app.toolbar;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ProgressBar;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.app.R;
import org.chromium.blimp_public.contents.BlimpContents;
import org.chromium.blimp_public.contents.BlimpContentsObserver;

/**
 * A {@link View} that visually represents the Blimp toolbar, which lets users issue navigation
 * commands and displays relevant navigation UI.
 */
@JNINamespace("blimp::client")
public class Toolbar extends LinearLayout
        implements UrlBar.UrlBarObserver, View.OnClickListener, BlimpContentsObserver {
    private Context mContext;
    private UrlBar mUrlBar;
    private ToolbarMenu mToolbarMenu;
    private ImageButton mReloadButton;
    private ImageButton mMenuButton;
    private ProgressBar mProgressBar;
    private BlimpContents mBlimpContents;

    /**
     * A URL to load when this object is initialized.  This handles the case where there is a URL
     * to load before native is ready to receive any URL.
     * */
    private String mUrlToLoad;

    /**
     * Builds a new {@link Toolbar}.
     * @param context A {@link Context} instance.
     * @param attrs   An {@link AttributeSet} instance.
     */
    public Toolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    /**
     * @return the mToolbarMenu
     */
    public ToolbarMenu getToolbarMenu() {
        return mToolbarMenu;
    }

    /**
     * To be called when the native library is loaded so that this class can initialize its native
     * components.
     * @param blimpContents The {@link BlimpContents} that represents the web contents.
     *        delegate The delegate for the Toolbar.
     */
    public void initialize(BlimpContents blimpContents) {
        mBlimpContents = blimpContents;
        mBlimpContents.addObserver(this);

        sendUrlTextInternal(mUrlToLoad);

        mToolbarMenu = new ToolbarMenu(mContext, this);
    }

    /**
     * Loads {@code text} as if it had been typed by the user.  Useful for specifically loading
     * startup URLs or testing.
     * @param text The URL or text to load.
     */
    public void loadUrl(String text) {
        mUrlBar.setText(text);
        sendUrlTextInternal(text);
    }

    /**
     * Returns the URL from the URL bar.
     * @return Current URL
     */
    public String getUrl() {
        return mUrlBar.getText().toString();
    }

    /**
     * To be called when the user triggers a back navigation action.
     */
    public void onBackPressed() {
        if (mBlimpContents == null) return;
        mBlimpContents.getNavigationController().goBack();
    }

    /**
     * To be called when the user triggers a forward navigation action.
     */
    public void onForwardPressed() {
        if (mBlimpContents == null) return;
        mBlimpContents.getNavigationController().goForward();
    }

    // View overrides.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mUrlBar = (UrlBar) findViewById(R.id.toolbar_url_bar);
        mUrlBar.addUrlBarObserver(this);

        mReloadButton = (ImageButton) findViewById(R.id.toolbar_reload_btn);
        mReloadButton.setOnClickListener(this);

        mMenuButton = (ImageButton) findViewById(R.id.menu_button);
        mMenuButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mToolbarMenu.showMenu(v);
            }
        });

        mProgressBar = (ProgressBar) findViewById(R.id.page_load_progress);
        mProgressBar.setVisibility(View.GONE);
    }

    // UrlBar.UrlBarObserver interface.
    @Override
    public void onNewTextEntered(String text) {
        sendUrlTextInternal(text);
    }

    // View.OnClickListener interface.
    @Override
    public void onClick(View view) {
        if (mBlimpContents == null) return;
        if (view == mReloadButton) mBlimpContents.getNavigationController().reload();
    }

    private void sendUrlTextInternal(String text) {
        mUrlToLoad = null;
        if (TextUtils.isEmpty(text)) return;

        if (mBlimpContents == null) {
            mUrlToLoad = text;
            return;
        }

        mBlimpContents.getNavigationController().loadUrl(text);

        // When triggering a navigation to a new URL, show the progress bar.
        // TODO(khushalsagar): We need more signals to hide the bar when the load might have failed.
        // For instance, in the case of a wrong URL right now or if there is no network connection.
        updateProgressBar(true);
    }

    private void updateProgressBar(boolean show) {
        if (show) {
            mProgressBar.setVisibility(View.VISIBLE);
        } else {
            mProgressBar.setVisibility(View.GONE);
        }
    }

    @Override
    public void onNavigationStateChanged() {
        if (mBlimpContents == null) return;
        String url = mBlimpContents.getNavigationController().getUrl();
        if (url != null) mUrlBar.setText(url);
    }

    @Override
    public void onLoadingStateChanged(boolean loading) {
        updateProgressBar(loading);
    }

    @Override
    public void onPageLoadingStateChanged(boolean loading) {
        updateProgressBar(loading);
    }
}
