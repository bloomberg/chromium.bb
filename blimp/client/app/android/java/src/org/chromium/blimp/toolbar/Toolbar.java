// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.toolbar;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ListPopupWindow;
import android.widget.ProgressBar;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.R;
import org.chromium.blimp.session.BlimpClientSession;
import org.chromium.blimp.session.EngineInfo;
import org.chromium.blimp.settings.AboutBlimpPreferences;
import org.chromium.blimp.settings.Preferences;

/**
 * A {@link View} that visually represents the Blimp toolbar, which lets users issue navigation
 * commands and displays relevant navigation UI.
 */
@JNINamespace("blimp::client")
public class Toolbar extends LinearLayout implements UrlBar.UrlBarObserver, View.OnClickListener,
                                                     BlimpClientSession.ConnectionObserver {
    private static final String TAG = "Toolbar";
    private static final int ID_OPEN_IN_CHROME = 0;
    private static final int ID_VERSION_INFO = 1;

    private long mNativeToolbarPtr;

    private Context mContext;
    private UrlBar mUrlBar;
    private ImageButton mReloadButton;
    private ImageButton mMenuButton;
    private ListPopupWindow mPopupMenu;
    private ProgressBar mProgressBar;

    private EngineInfo mEngineInfo;

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
     * To be called when the native library is loaded so that this class can initialize its native
     * components.
     * @param blimpClientSession The {@link BlimpClientSession} that contains the content-lite
     *                           features required by the native components of the Toolbar.
     */
    public void initialize(BlimpClientSession blimpClientSession) {
        assert mNativeToolbarPtr == 0;

        mNativeToolbarPtr = nativeInit(blimpClientSession);
        sendUrlTextInternal(mUrlToLoad);
    }

    /**
     * To be called when this class should be torn down.  This {@link View} should not be used after
     * this.
     */
    public void destroy() {
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
        sendUrlTextInternal(text);
    }

    /**
     * To be called when the user triggers a back navigation action.
     * @return Whether or not the back event was consumed.
     */
    public boolean onBackPressed() {
        if (mNativeToolbarPtr == 0) return false;
        return nativeOnBackPressed(mNativeToolbarPtr);
    }

    /**
     * To be called when the user triggers a forward navigation action.
     */
    public void onForwardPressed() {
        if (mNativeToolbarPtr == 0) return;
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
                showMenu(v);
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

    private void showMenu(View anchorView) {
        if (mPopupMenu == null) {
            initializeMenu(anchorView);
        }
        mPopupMenu.show();
        mPopupMenu.getListView().setDivider(null);
    }

    /**
     * Creates and initializes the app menu anchored to the specified view.
     * @param anchorView The anchor of the {@link ListPopupWindow}
     */
    private void initializeMenu(View anchorView) {
        mPopupMenu = new ListPopupWindow(mContext);
        mPopupMenu.setAdapter(new ArrayAdapter<String>(mContext, R.layout.toolbar_popup_item,
                new String[] {mContext.getString(R.string.open_in_chrome),
                        mContext.getString(R.string.version_info)}));
        mPopupMenu.setAnchorView(anchorView);
        mPopupMenu.setWidth(getResources().getDimensionPixelSize(R.dimen.toolbar_popup_item_width));
        mPopupMenu.setVerticalOffset(-anchorView.getHeight());
        mPopupMenu.setModal(true);
        mPopupMenu.setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if (position == ID_OPEN_IN_CHROME) {
                    openInChrome();
                } else if (position == ID_VERSION_INFO) {
                    showVersionInfo();
                }
                mPopupMenu.dismiss();
            }
        });
    }

    private void openInChrome() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(mUrlBar.getText().toString()));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage("com.android.chrome");
        try {
            mContext.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            // Chrome is probably not installed, so try with the default browser
            intent.setPackage(null);
            mContext.startActivity(intent);
        }
    }

    private void showVersionInfo() {
        Intent intent = new Intent();
        intent.setClass(mContext, Preferences.class);
        if (mEngineInfo != null) {
            intent.putExtra(AboutBlimpPreferences.EXTRA_ENGINE_IP, mEngineInfo.ipAddress);
            intent.putExtra(AboutBlimpPreferences.EXTRA_ENGINE_VERSION, mEngineInfo.engineVersion);
        }
        mContext.startActivity(intent);
    }

    // BlimpClientSession.ConnectionObserver interface.
    @Override
    public void onAssignmentReceived(
            int result, int suggestedMessageResourceId, EngineInfo engineInfo) {
        mEngineInfo = engineInfo;
    }

    @Override
    public void onConnected() {
        if (mEngineInfo == null) return;

        mEngineInfo.setConnected(true);
    }

    @Override
    public void onDisconnected(String reason) {
        if (mEngineInfo == null) return;

        mEngineInfo.setConnected(false);
    }

    // Methods that are called by native via JNI.
    @CalledByNative
    private void onEngineSentUrl(String url) {
        if (url != null) mUrlBar.setText(url);
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
