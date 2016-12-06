// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.app;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.widget.RelativeLayout;

import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.blimp.app.toolbar.Toolbar;
import org.chromium.blimp_public.BlimpClientContext;
import org.chromium.blimp_public.BlimpClientContextDelegate;
import org.chromium.blimp_public.contents.BlimpContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * The {@link Activity} for rendering the main Blimp client.  This loads the Blimp rendering stack
 * and displays it.
 */
public class BlimpRendererActivity
        extends Activity implements BlimpLibraryLoader.Callback, BlimpClientContextDelegate {
    private static final String TAG = "BlimpRendActivity";

    private BlimpContentsDisplay mBlimpContentsDisplay;
    private Toolbar mToolbar;
    private WindowAndroid mWindowAndroid;
    private BlimpEnvironment mBlimpEnvironment;

    private boolean mFirstUrlLoadDone = false;

    // Flag to record the base value of the metrics when the debug view is turned on.
    private BlimpContents mBlimpContents;
    private BlimpClientContext mBlimpClientContext;

    @Override
    @SuppressFBWarnings("DM_EXIT") // FindBugs doesn't like System.exit().
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            BlimpLibraryLoader.startAsync(this);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Native startup exception", e);
            System.exit(-1);
            return;
        }
    }

    @Override
    protected void onDestroy() {
        if (mBlimpContentsDisplay != null) {
            mBlimpContentsDisplay.destroyRenderer();
            mBlimpContentsDisplay = null;
        }

        if (mBlimpContents != null) {
            mBlimpContents.destroy();
            mBlimpContents = null;
        }

        mBlimpClientContext = null;

        if (mBlimpEnvironment != null) {
            mBlimpEnvironment.destroy();
            mBlimpEnvironment = null;
        }

        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        // Check if the toolbar can handle the back navigation.
        if (mToolbar != null) {
            mToolbar.onBackPressed();
            return;
        }

        // If not, use the default Activity behavior.
        super.onBackPressed();
    }

    // BlimpLibraryLoader.Callback implementation.
    @Override
    public void onStartupComplete(boolean success) {
        if (!success) {
            Log.e(TAG, "Native startup failed");
            finish();
            return;
        }

        setContentView(R.layout.blimp_main);

        mWindowAndroid = new WindowAndroid(BlimpRendererActivity.this);

        mBlimpEnvironment = BlimpEnvironment.getInstance();

        mBlimpClientContext = mBlimpEnvironment.getBlimpClientContext();
        mBlimpContents = mBlimpClientContext.createBlimpContents(mWindowAndroid);
        mBlimpContentsDisplay = (BlimpContentsDisplay) findViewById(R.id.contents_display);
        mBlimpContentsDisplay.initializeRenderer(mBlimpEnvironment, mBlimpContents);

        RelativeLayout.LayoutParams params =
                new RelativeLayout.LayoutParams(mBlimpContentsDisplay.getLayoutParams());
        params.addRule(RelativeLayout.BELOW, R.id.toolbar);
        ((ViewGroup) findViewById(R.id.container)).addView(mBlimpContents.getView(), params);

        mToolbar = (Toolbar) findViewById(R.id.toolbar);
        mToolbar.initialize(mBlimpContents);

        mBlimpClientContext.connect();

        handleUrlFromIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleUrlFromIntent(intent);
    }

    /**
     * Retrieve the URL from the Intent.
     * @param intent Intent to examine.
     * @return URL from the Intent, or null if a valid URL couldn't be found.
     */
    private String getUrlFromIntent(Intent intent) {
        if (intent == null) return null;

        String url = intent.getDataString();
        if (url == null) return null;

        url = url.trim();
        return TextUtils.isEmpty(url) ? null : url;
    }

    /**
     * Retrieves an URL from an Intent and loads it in the browser.
     * If the toolbar already has an URL and the new intent doesn't have an URL (e.g. bringing back
     * from background), the intent gets ignored.
     * @param intent Intent that contains the URL.
     */
    private void handleUrlFromIntent(Intent intent) {
        // TODO(shaktisahu): On a slow device, this might happen. Load the correct URL once the
        // toolbar loading is complete (crbug/601226)
        if (mToolbar == null) return;

        String url = getUrlFromIntent(intent);
        if (mFirstUrlLoadDone && url == null) return;
        mFirstUrlLoadDone = true;

        mToolbar.loadUrl(url == null ? "http://www.google.com/" : url);
    }

    @Override
    public void restartBrowser() {
        Intent intent = BrowserRestartActivity.createRestartIntent(this);
        startActivity(intent);
    }

    @Override
    public void startUserSignInFlow(Context context) {}
}
