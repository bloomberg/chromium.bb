// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.blimp.auth.RetryingTokenSource;
import org.chromium.blimp.auth.TokenSource;
import org.chromium.blimp.auth.TokenSourceImpl;
import org.chromium.blimp.input.WebInputBox;
import org.chromium.blimp.preferences.PreferencesUtil;
import org.chromium.blimp.session.BlimpClientSession;
import org.chromium.blimp.session.EngineInfo;
import org.chromium.blimp.session.TabControlFeature;
import org.chromium.blimp.toolbar.Toolbar;
import org.chromium.ui.widget.Toast;

/**
 * The {@link Activity} for rendering the main Blimp client.  This loads the Blimp rendering stack
 * and displays it.
 */
public class BlimpRendererActivity
        extends Activity implements BlimpLibraryLoader.Callback, TokenSource.Callback,
                                    BlimpClientSession.ConnectionObserver {
    private static final int ACCOUNT_CHOOSER_INTENT_REQUEST_CODE = 100;
    private static final String TAG = "BlimpRendererActivity";

    /** Provides user authentication tokens that can be used to query for engine assignments.  This
     *  can potentially query GoogleAuthUtil for an OAuth2 authentication token with userinfo.email
     *  privileges for a chosen Android account. */
    private TokenSource mTokenSource;

    private BlimpView mBlimpView;
    private Toolbar mToolbar;
    private BlimpClientSession mBlimpClientSession;
    private TabControlFeature mTabControlFeature;
    private WebInputBox mWebInputBox;

    @Override
    @SuppressFBWarnings("DM_EXIT")  // FindBugs doesn't like System.exit().
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Build a TokenSource that will internally retry accessing the underlying TokenSourceImpl.
        // This will exponentially backoff while it tries to get the access token.  See
        // {@link RetryingTokenSource} for more information.  The underlying
        // TokenSourceImpl will attempt to query GoogleAuthUtil, but might fail if there is no
        // account selected, in which case it will ask this Activity to show an account chooser and
        // notify it of the selection result.
        mTokenSource = new RetryingTokenSource(new TokenSourceImpl(this));
        mTokenSource.setCallback(this);
        mTokenSource.getToken();

        try {
            BlimpLibraryLoader.startAsync(this, this);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Native startup exception", e);
            System.exit(-1);
            return;
        }
    }

    @Override
    protected void onDestroy() {
        if (mTabControlFeature != null) {
            mTabControlFeature.destroy();
            mTabControlFeature = null;
        }

        if (mBlimpView != null) {
            mBlimpView.destroyRenderer();
            mBlimpView = null;
        }

        if (mToolbar != null) {
            if (mBlimpClientSession != null) {
                mBlimpClientSession.removeObserver(mToolbar);
            }
            mToolbar.destroy();
            mToolbar = null;
        }

        if (mWebInputBox != null) {
            mWebInputBox.destroy();
            mWebInputBox = null;
        }

        if (mTokenSource != null) {
            mTokenSource.destroy();
            mTokenSource = null;
        }

        // Destroy the BlimpClientSession last, as all other features may rely on it.
        if (mBlimpClientSession != null) {
            mBlimpClientSession.removeObserver(this);
            mBlimpClientSession.destroy();
            mBlimpClientSession = null;
        }

        super.onDestroy();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case ACCOUNT_CHOOSER_INTENT_REQUEST_CODE:
                if (resultCode == RESULT_OK) {
                    mTokenSource.onAccountSelected(data);
                    mTokenSource.getToken();
                } else {
                    onTokenUnavailable(false);
                }
                break;
            default:
                break;
        }
    }

    @Override
    public void onBackPressed() {
        // Check if the toolbar can handle the back navigation.
        if (mToolbar != null && mToolbar.onBackPressed()) return;

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

        mBlimpClientSession = new BlimpClientSession(PreferencesUtil.findAssignerUrl(this));
        mBlimpClientSession.addObserver(this);

        mBlimpView = (BlimpView) findViewById(R.id.renderer);
        mBlimpView.initializeRenderer(mBlimpClientSession);

        mToolbar = (Toolbar) findViewById(R.id.toolbar);
        mToolbar.initialize(mBlimpClientSession);
        mBlimpClientSession.addObserver(mToolbar);

        mWebInputBox = (WebInputBox) findViewById(R.id.editText);
        mWebInputBox.initialize(mBlimpClientSession);

        mTabControlFeature = new TabControlFeature(mBlimpClientSession, mBlimpView);

        handleUrl(getUrlFromIntent(getIntent()));
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleUrl(getUrlFromIntent(intent));
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
     * Loads the url in the browser.
     * @param url URL to load. If null, browser loads a default page.
     */
    private void handleUrl(String url) {
        // TODO(shaktisahu): On a slow device, this might happen. Load the correct URL once the
        // toolbar loading is complete (crbug/601226)
        if (mToolbar == null) return;

        if (url == null) {
            mToolbar.loadUrl("http://www.google.com/");
        } else {
            mToolbar.loadUrl(url);
        }
    }

    // TokenSource.Callback implementation.
    @Override
    public void onTokenReceived(String token) {
        if (mBlimpClientSession != null) mBlimpClientSession.connect(token);
    }

    @Override
    public void onTokenUnavailable(boolean isTransient) {
        // Ignore isTransient here because we're relying on the auto-retry TokenSource.
        // TODO(dtrainor): Show a better error dialog/message.
        Toast.makeText(this, R.string.signin_get_token_failed, Toast.LENGTH_LONG).show();
    }

    @Override
    public void onNeedsAccountToBeSelected(Intent suggestedIntent) {
        startActivityForResult(suggestedIntent, ACCOUNT_CHOOSER_INTENT_REQUEST_CODE);
    }

    // BlimpClientSession.ConnectionObserver interface.
    @Override
    public void onAssignmentReceived(
            int result, int suggestedMessageResourceId, EngineInfo engineInfo) {
        Toast.makeText(this, suggestedMessageResourceId, Toast.LENGTH_LONG).show();
    }

    @Override
    public void onConnected() {
        Toast.makeText(this, R.string.network_connected, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onDisconnected(String reason) {
        Toast.makeText(this,
                     String.format(getResources().getString(R.string.network_disconnected), reason),
                     Toast.LENGTH_LONG)
                .show();
    }
}
