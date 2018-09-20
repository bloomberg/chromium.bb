// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.List;

import javax.annotation.Nullable;

/** Delegate to interact with the assistant UI. */
class AutofillAssistantUiDelegate {
    private final Activity mActivity;
    private final Client mClient;
    private final View mFullContainer;
    private final View mOverlay;
    private final LinearLayout mBottomBar;
    private final ViewGroup mScriptsViewContainer;
    private final TextView mStatusMessageView;

    /**
     * This is a client interface that relays interactions from the UI.
     *
     * Java version of the native autofill_assistant::UiDelegate.
     */
    public interface Client {
        /**
         * Called when clicking on the overlay.
         */
        void onClickOverlay();

        /**
         * Called when the bottom bar is dismissing.
         */
        void onDismiss();

        /**
         * Called when a script has been selected.
         *
         * @param scriptPath The path for the selected script.
         */
        void onScriptSelected(String scriptPath);
    }

    /**
     * Java side equivalent of autofill_assistant::ScriptHandle.
     */
    protected static class ScriptHandle {
        /** The display name of this script. */
        private final String mName;
        /** The script path. */
        private final String mPath;

        /** Constructor. */
        public ScriptHandle(String name, String path) {
            mName = name;
            mPath = path;
        }

        /** Returns the display name. */
        public String getName() {
            return mName;
        }

        /** Returns the script path. */
        public String getPath() {
            return mPath;
        }
    }

    /**
     * Constructs an assistant UI delegate.
     *
     * @param activity The Activity
     * @param client The client to forward events to
     */
    public AutofillAssistantUiDelegate(Activity activity, Client client) {
        mActivity = activity;
        mClient = client;

        mFullContainer = LayoutInflater.from(mActivity)
                                 .inflate(R.layout.autofill_assistant_sheet,
                                         ((ViewGroup) mActivity.findViewById(R.id.coordinator)))
                                 .findViewById(R.id.autofill_assistant);
        // TODO(crbug.com/806868): Set hint text on overlay.
        mOverlay = mFullContainer.findViewById(R.id.overlay);
        mOverlay.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mClient.onClickOverlay();
            }
        });
        mBottomBar = mFullContainer.findViewById(R.id.bottombar);
        mBottomBar.findViewById(R.id.close_button).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mFullContainer.setVisibility(View.GONE);
                mClient.onDismiss();
            }
        });
        mBottomBar.findViewById(R.id.feedback_button)
                .setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        // TODO(crbug.com/806868): Send feedback.
                    }
                });
        mScriptsViewContainer = mBottomBar.findViewById(R.id.carousel);
        mStatusMessageView = mBottomBar.findViewById(R.id.status_message);

        // TODO(crbug.com/806868): Listen for contextual search shown so as to hide this UI.
    }

    /**
     * Shows a message in the status bar.
     *
     * @param message Message to display.
     */
    public void showStatusMessage(@Nullable String message) {
        if (!mFullContainer.isShown()) mFullContainer.setVisibility(View.VISIBLE);

        mStatusMessageView.setText(message);
    }

    /**
     * Updates the list of scripts in the bar.
     *
     * @param scripts List of scripts to show.
     */
    public void updateScripts(List<ScriptHandle> scriptHandles) {
        mScriptsViewContainer.removeAllViews();

        if (scriptHandles.isEmpty()) {
            return;
        }

        for (ScriptHandle scriptHandle : scriptHandles) {
            TextView scriptView = (TextView) (LayoutInflater.from(mActivity).inflate(
                    R.layout.autofill_assistant_chip, null));
            scriptView.setText(scriptHandle.getName());
            scriptView.setOnClickListener(
                    (unusedView) -> { mClient.onScriptSelected(scriptHandle.getPath()); });
            mScriptsViewContainer.addView(scriptView);
        }

        if (!mFullContainer.isShown()) mFullContainer.setVisibility(View.VISIBLE);
    }

    /** Called to show overlay. */
    public void showOverlay() {
        mOverlay.setVisibility(View.VISIBLE);
    }

    /** Called to hide overlay. */
    public void hideOverlay() {
        mOverlay.setVisibility(View.GONE);
    }
}
