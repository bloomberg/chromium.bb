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
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.ArrayList;

import javax.annotation.Nullable;

/** Delegate to interact with the assistant UI. */
class AutofillAssistantUiDelegate {
    private final Activity mActivity;
    private final Client mClient;
    private final View mFullContainer;
    private final View mOverlay;
    private final LinearLayout mBottomBar;
    private final ViewGroup mChipsViewContainer;
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

        /**
         * Called when an address has been selected.
         *
         * @param guid The GUID of the selected address.
         */
        void onAddressSelected(String guid);

        /**
         * Called when a credit card has been selected.
         *
         * @param guid The GUID of the selected card.
         */
        void onCardSelected(String guid);
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
                shutdown();
            }
        });
        mBottomBar.findViewById(R.id.feedback_button)
                .setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        // TODO(crbug.com/806868): Send feedback.
                    }
                });
        mChipsViewContainer = mBottomBar.findViewById(R.id.carousel);
        mStatusMessageView = mBottomBar.findViewById(R.id.status_message);

        // TODO(crbug.com/806868): Listen for contextual search shown so as to hide this UI.
    }

    /**
     * Shows a message in the status bar.
     *
     * @param message Message to display.
     */
    public void showStatusMessage(@Nullable String message) {
        ensureFullContainerIsShown();

        mStatusMessageView.setText(message);
    }

    private void ensureFullContainerIsShown() {
        if (!mFullContainer.isShown()) mFullContainer.setVisibility(View.VISIBLE);
    }

    /**
     * Updates the list of scripts in the bar.
     *
     * @param scriptHandles List of scripts to show.
     */
    public void updateScripts(ArrayList<ScriptHandle> scriptHandles) {
        mChipsViewContainer.removeAllViews();

        if (scriptHandles.isEmpty()) {
            return;
        }

        for (int i = 0; i < scriptHandles.size(); i++) {
            ScriptHandle scriptHandle = scriptHandles.get(i);
            TextView chipView = createChipView(scriptHandle.getName());
            chipView.setOnClickListener((unusedView) -> {
                mChipsViewContainer.removeAllViews();
                mClient.onScriptSelected(scriptHandle.getPath());
            });
            mChipsViewContainer.addView(chipView);
        }

        ensureFullContainerIsShown();
    }

    private TextView createChipView(String text) {
        TextView chipView = (TextView) (LayoutInflater.from(mActivity).inflate(
                R.layout.autofill_assistant_chip, null /* root */));
        chipView.setText(text);
        return chipView;
    }

    /** Called to show overlay. */
    public void showOverlay() {
        mOverlay.setVisibility(View.VISIBLE);
    }

    /** Called to hide overlay. */
    public void hideOverlay() {
        mOverlay.setVisibility(View.GONE);
    }

    /**
     * Shuts down the Autofill Assistant. The UI disappears and any associated state goes away.
     */
    public void shutdown() {
        mFullContainer.setVisibility(View.GONE);
        mClient.onDismiss();
    }

    /**
     * Show profiles in the bar.
     *
     * @param profiles List of profiles to show.
     */
    public void showProfiles(ArrayList<AutofillProfile> profiles) {
        if (profiles.isEmpty()) {
            mClient.onAddressSelected("");
            return;
        }

        mChipsViewContainer.removeAllViews();

        for (int i = 0; i < profiles.size(); i++) {
            AutofillProfile profile = profiles.get(i);
            // TODO(crbug.com/806868): Show more information than the street.
            TextView chipView = createChipView(profile.getStreetAddress());
            chipView.setOnClickListener((unusedView) -> {
                mChipsViewContainer.removeAllViews();
                mClient.onAddressSelected(profile.getGUID());
            });
            mChipsViewContainer.addView(chipView);
        }

        ensureFullContainerIsShown();
    }

    /**
     * Show credit cards in the bar.
     *
     * @param cards List of cards to show.
     */
    public void showCards(ArrayList<CreditCard> cards) {
        if (cards.isEmpty()) {
            mClient.onCardSelected("");
            return;
        }

        mChipsViewContainer.removeAllViews();

        for (int i = 0; i < cards.size(); i++) {
            CreditCard card = cards.get(i);
            // TODO(crbug.com/806868): Show more information than the card number.
            TextView chipView = createChipView(card.getObfuscatedNumber());
            chipView.setOnClickListener((unusedView) -> {
                mChipsViewContainer.removeAllViews();
                mClient.onCardSelected(card.getGUID());
            });
            mChipsViewContainer.addView(chipView);
        }

        ensureFullContainerIsShown();
    }
}
