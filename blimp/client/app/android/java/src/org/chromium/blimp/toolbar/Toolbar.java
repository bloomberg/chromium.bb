// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.toolbar;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ProgressBar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.R;
import org.chromium.blimp.session.BlimpClientSession;

/**
 * A {@link View} that visually represents the Blimp toolbar, which lets users issue navigation
 * commands and displays relevant navigation UI.
 */
@JNINamespace("blimp::client")
public class Toolbar extends LinearLayout implements UrlBar.UrlBarObserver, View.OnClickListener {
    /**
     * Delegate for the Toolbar.
     */
    public interface ToolbarDelegate {
        /**
         * Resets the metrics. Used for displaying per navigation metrics.
         */
        public void resetDebugStats();
    }

    private static final String TAG = "Toolbar";

    private long mNativeToolbarPtr;

    private Context mContext;
    private UrlBar mUrlBar;
    private ToolbarMenu mToolbarMenu;
    private ImageButton mReloadButton;
    private ImageButton mMenuButton;
    private ProgressBar mProgressBar;
    private BlimpClientSession mBlimpClientSession;
    private ToolbarDelegate mDelegate;

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
     * @param blimpClientSession The {@link BlimpClientSession} that contains the content-lite
     *                           features required by the native components of the Toolbar.
     *        delegate The delegate for the Toolbar.
     */
    public void initialize(BlimpClientSession blimpClientSession, ToolbarDelegate delegate) {
        assert mNativeToolbarPtr == 0;

        mDelegate = delegate;

        mBlimpClientSession = blimpClientSession;
        mNativeToolbarPtr = nativeInit(mBlimpClientSession);
        sendUrlTextInternal(mUrlToLoad);

        mToolbarMenu = new ToolbarMenu(mContext, this);
        mBlimpClientSession.addObserver(mToolbarMenu);
    }

    /**
     * To be called when this class should be torn down.  This {@link View} should not be used after
     * this.
     */
    public void destroy() {
        mBlimpClientSession.removeObserver(mToolbarMenu);
        if (mNativeToolbarPtr != 0) {
            nativeDestroy(mNativeToolbarPtr);
            mNativeToolbarPtr = 0;
        }
    }

    /**
     * Loads {@code text} as if it had been typed by the user.  Useful for specifically loading
     * startup URLs or testing.
     * @param text The URL or text to load.
     */
    public void loadUrl(String text) {
        mUrlBar.setText(text);
        mDelegate.resetDebugStats();
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
     * @return Whether or not the back event was consumed.
     */
    public boolean onBackPressed() {
        if (mNativeToolbarPtr == 0) return false;
        mDelegate.resetDebugStats();
        return nativeOnBackPressed(mNativeToolbarPtr);
    }

    /**
     * To be called when the user triggers a forward navigation action.
     */
    public void onForwardPressed() {
        if (mNativeToolbarPtr == 0) return;
        mDelegate.resetDebugStats();
        nativeOnForwardPressed(mNativeToolbarPtr);
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
        if (mNativeToolbarPtr == 0) return;
        if (view == mReloadButton) nativeOnReloadPressed(mNativeToolbarPtr);
    }

    private void sendUrlTextInternal(String text) {
        mUrlToLoad = null;
        if (TextUtils.isEmpty(text)) return;

        if (mNativeToolbarPtr == 0) {
            mUrlToLoad = text;
            return;
        }

        nativeOnUrlTextEntered(mNativeToolbarPtr, text);

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

    // Methods that are called by native via JNI.
    @CalledByNative
    private void onEngineSentUrl(String url) {
        if (url != null) mUrlBar.setText(url);
        mDelegate.resetDebugStats();
    }

    @CalledByNative
    private void onEngineSentFavicon(Bitmap favicon) {
        // TODO(dtrainor): Add a UI for the favicon.
    }

    @CalledByNative
    private void onEngineSentTitle(String title) {
        // TODO(dtrainor): Add a UI for the title.
    }

    @CalledByNative
    private void onEngineSentLoading(boolean loading) {
        // TODO(dtrainor): Add a UI for the loading state.
    }

    @CalledByNative
    private void onEngineSentPageLoadStatusUpdate(boolean completed) {
        boolean show = !completed;
        updateProgressBar(show);
    }

    private native long nativeInit(BlimpClientSession blimpClientSession);
    private native void nativeDestroy(long nativeToolbar);
    private native void nativeOnUrlTextEntered(long nativeToolbar, String text);
    private native void nativeOnReloadPressed(long nativeToolbar);
    private native void nativeOnForwardPressed(long nativeToolbar);
    private native boolean nativeOnBackPressed(long nativeToolbar);
}
