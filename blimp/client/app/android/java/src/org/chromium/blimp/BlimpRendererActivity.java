// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.CommandLine;
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
import org.chromium.blimp.toolbar.ToolbarMenu;
import org.chromium.ui.widget.Toast;

/**
 * The {@link Activity} for rendering the main Blimp client.  This loads the Blimp rendering stack
 * and displays it.
 */
public class BlimpRendererActivity
        extends Activity implements BlimpLibraryLoader.Callback, TokenSource.Callback,
                                    BlimpClientSession.ConnectionObserver,
                                    ToolbarMenu.ToolbarMenuDelegate, Toolbar.ToolbarDelegate {
    private static final int ACCOUNT_CHOOSER_INTENT_REQUEST_CODE = 100;
    private static final String TAG = "BlimpRendererActivity";

    // Refresh interval for the debug view in milliseconds.
    private static final int DEBUG_VIEW_REFRESH_INTERVAL = 1000;
    private static final int BYTES_PER_KILO = 1024;

    /** Provides user authentication tokens that can be used to query for engine assignments.  This
     *  can potentially query GoogleAuthUtil for an OAuth2 authentication token with userinfo.email
     *  privileges for a chosen Android account. */
    private TokenSource mTokenSource;

    private BlimpView mBlimpView;
    private Toolbar mToolbar;
    private BlimpClientSession mBlimpClientSession;
    private TabControlFeature mTabControlFeature;
    private WebInputBox mWebInputBox;

    private Handler mHandler = new Handler();

    private boolean mFirstUrlLoadDone = false;

    // Flag to record the base value of the metrics when the debug view is turned on.
    private boolean mStatsBaseRecorded = false;
    private int mSentBase;
    private int mReceivedBase;
    private int mCommitsBase;
    private int mSent;
    private int mReceived;
    private int mCommits;
    private String mToken = null;

    @Override
    @SuppressFBWarnings("DM_EXIT")  // FindBugs doesn't like System.exit().
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        buildAndTriggerTokenSourceIfNeeded();

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
        mToolbar.initialize(mBlimpClientSession, this);

        mWebInputBox = (WebInputBox) findViewById(R.id.editText);
        mWebInputBox.initialize(mBlimpClientSession);

        mTabControlFeature = new TabControlFeature(mBlimpClientSession, mBlimpView);

        handleUrlFromIntent(getIntent());

        // If Blimp client has command line flag "engine-ip", client will use the command line token
        // to connect. See GetAssignmentFromCommandLine() in
        // blimp/client/session/assignment_source.cc
        // In normal cases, where client uses the engine ip given by the Assigner,
        // connection to the engine is triggered by the successful retrieval of a token as
        // TokenSource.Callback.
        if (CommandLine.getInstance().hasSwitch(BlimpClientSwitches.ENGINE_IP)) {
            mBlimpClientSession.connect(null);
        } else {
            if (mToken != null) {
                mBlimpClientSession.connect(mToken);
                mToken = null;
            }
        }
    }

    // ToolbarMenu.ToolbarMenuDelegate implementation.
    @Override
    public void showDebugView(boolean show) {
        View debugView = findViewById(R.id.debug_stats);
        debugView.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
        if (show) {
            Runnable debugStatsRunnable = new Runnable() {
                @Override
                public void run() {
                    if (mToolbar.getToolbarMenu().isDebugInfoEnabled()) {
                        int[] metrics = mBlimpClientSession.getDebugStats();
                        updateDebugStats(metrics);
                        mHandler.postDelayed(this, DEBUG_VIEW_REFRESH_INTERVAL);
                    }
                }
            };
            debugStatsRunnable.run();
        } else {
            mStatsBaseRecorded = false;
        }
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

    // TokenSource.Callback implementation.
    @Override
    public void onTokenReceived(String token) {
        if (mBlimpClientSession != null) {
            mBlimpClientSession.connect(token);
        } else {
            mToken = token;
        }
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

    /**
     * Displays debug metrics up to one decimal place.
     */
    @Override
    public void updateDebugStatsUI(int received, int sent, int commits) {
        TextView tv = (TextView) findViewById(R.id.bytes_received_client);
        tv.setText(String.format("%.1f", (float) received / BYTES_PER_KILO));
        tv = (TextView) findViewById(R.id.bytes_sent_client);
        tv.setText(String.format("%.1f", (float) sent / BYTES_PER_KILO));
        tv = (TextView) findViewById(R.id.commit_count);
        tv.setText(String.valueOf(commits));
    }

    private void updateDebugStats(int[] metrics) {
        assert metrics.length == 3;
        mReceived = metrics[0];
        mSent = metrics[1];
        mCommits = metrics[2];
        if (!mStatsBaseRecorded) {
            mReceivedBase = mReceived;
            mSentBase = mSent;
            mCommitsBase = mCommits;
            mStatsBaseRecorded = true;
        }
        updateDebugStatsUI(mReceived - mReceivedBase, mSent - mSentBase, mCommits - mCommitsBase);
    }

    // Toolbar.ToolbarDelegate interface.
    @Override
    public void resetDebugStats() {
        mReceivedBase = mReceived;
        mSentBase = mSent;
        mCommitsBase = mCommits;
    }

    @Override
    public void onDisconnected(String reason) {
        Toast.makeText(this,
                     String.format(getResources().getString(R.string.network_disconnected), reason),
                     Toast.LENGTH_LONG)
                .show();
    }

    private void buildAndTriggerTokenSourceIfNeeded() {
        // If Blimp client is given the engine ip by the command line, then there is no need to
        // build a TokenSource, because token, engine ip, engine port, and transport protocol are
        // all given by command line.
        if (CommandLine.getInstance().hasSwitch(BlimpClientSwitches.ENGINE_IP)) return;

        // Build a TokenSource that will internally retry accessing the underlying
        // TokenSourceImpl. This will exponentially backoff while it tries to get the access
        // token.  See {@link RetryingTokenSource} for more information.  The underlying
        // TokenSourceImpl will attempt to query GoogleAuthUtil, but might fail if there is no
        // account selected, in which case it will ask this Activity to show an account chooser
        // and notify it of the selection result.
        mTokenSource = new RetryingTokenSource(new TokenSourceImpl(this));
        mTokenSource.setCallback(this);
        mTokenSource.getToken();
    }
}
