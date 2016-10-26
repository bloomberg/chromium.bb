// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.support.annotation.DrawableRes;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.v7.widget.RecyclerView;

import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPage.DestructionObserver;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.signin.AccountSigninActivity;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;

/**
 * Shows a card prompting the user to sign in. This item is also a {@link TreeNode}, and calling
 * {@link #hide()} or {@link #maybeShow()} will control its visibility.
 */
public class SignInPromo extends ChildNode implements StatusCardViewHolder.DataSource {
    /**
     * Whether the promo should be visible, according to the parent object.
     *
     * The {@link NewTabPageAdapter} calls to {@link #maybeShow()} and {@link #hide()} modify this
     * when the sign in status changes.
     */
    private boolean mVisible;

    /**
     * Whether the user has seen the promo and dismissed it at some point. When this is set,
     * the promo will never be shown.
     */
    private boolean mDismissed;

    @Nullable
    private final SigninObserver mObserver;

    public SignInPromo(NodeParent parent, NewTabPageAdapter adapter) {
        super(parent);
        mDismissed = ChromePreferenceManager.getInstance(ContextUtils.getApplicationContext())
                             .getNewTabPageSigninPromoDismissed();

        final SigninManager signinManager = SigninManager.get(ContextUtils.getApplicationContext());
        mObserver = mDismissed ? null : new SigninObserver(signinManager, adapter);
        mVisible = signinManager.isSignInAllowed() && !signinManager.isSignedInOnNative();
    }

    @Override
    public int getItemCount() {
        if (!isShown()) return 0;

        return 1;
    }

    @Override
    @ItemViewType
    public int getItemViewType(int position) {
        checkIndex(position);
        return ItemViewType.PROMO;
    }

    /**
     * @returns a {@link DestructionObserver} observer that updates the visibility of the signin
     * promo and unregisters itself when the New Tab Page is destroyed.
     */
    @Nullable
    public DestructionObserver getObserver() {
        return mObserver;
    }

    @Override
    public void onBindViewHolder(NewTabPageViewHolder holder, int position) {
        checkIndex(position);
        assert holder instanceof StatusCardViewHolder;
        ((StatusCardViewHolder) holder).onBindViewHolder(this);
    }

    @Override
    public SnippetArticle getSuggestionAt(int position) {
        checkIndex(position);
        return null;
    }

    @Override
    public int getDismissSiblingPosDelta(int position) {
        checkIndex(position);
        return 0;
    }

    @Override
    @StringRes
    public int getHeader() {
        return R.string.snippets_disabled_generic_prompt;
    }

    @Override
    public String getDescription() {
        return ContextUtils.getApplicationContext().getString(
                R.string.snippets_disabled_signed_out_instructions);
    }

    @Override
    @StringRes
    public int getActionLabel() {
        return R.string.sign_in_button;
    }

    @Override
    public void performAction(Context context) {
        AccountSigninActivity.startIfAllowed(context, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS);
    }

    public boolean isShown() {
        return !mDismissed && mVisible;
    }

    /** Attempts to show the sign in promo. If the user dismissed it before, it will not be shown.*/
    private void maybeShow() {
        if (mVisible) return;
        mVisible = true;

        if (mDismissed) return;

        RecordUserAction.record("Signin_Impression_FromNTPContentSuggestions");
        notifyItemInserted(0);
    }

    /** Hides the sign in promo. */
    private void hide() {
        if (!mVisible) return;
        mVisible = false;

        if (mDismissed) return;

        notifyItemRemoved(0);
    }

    /** Hides the sign in promo and sets a preference to make sure it is not shown again. */
    public void dismiss() {
        hide();
        mDismissed = true;
        ChromePreferenceManager.getInstance(ContextUtils.getApplicationContext())
                .setNewTabPageSigninPromoDismissed(true);
        mObserver.unregister();
    }

    @VisibleForTesting
    class SigninObserver
            implements SignInStateObserver, SignInAllowedObserver, DestructionObserver {
        private final SigninManager mSigninManager;
        private final NewTabPageAdapter mAdapter;

        /** Guards {@link #unregister()}, which can be called multiple times. */
        private boolean mUnregistered;

        private SigninObserver(SigninManager signinManager, NewTabPageAdapter adapter) {
            mSigninManager = signinManager;
            mAdapter = adapter;
            mSigninManager.addSignInAllowedObserver(this);
            mSigninManager.addSignInStateObserver(this);
        }

        private void unregister() {
            if (mUnregistered) return;
            mUnregistered = true;

            mSigninManager.removeSignInAllowedObserver(this);
            mSigninManager.removeSignInStateObserver(this);
        }

        @Override
        public void onDestroy() {
            unregister();
        }

        @Override
        public void onSignInAllowedChanged() {
            // Listening to onSignInAllowedChanged is important for the FRE. Sign in is not allowed
            // until it is completed, but the NTP is initialised before the FRE is even shown. By
            // implementing this we can show the promo if the user did not sign in during the FRE.
            if (mSigninManager.isSignInAllowed()) {
                maybeShow();
            } else {
                hide();
            }
        }

        @Override
        public void onSignedIn() {
            hide();
            mAdapter.resetSections(/*alwaysAllowEmptySections=*/false);
        }

        @Override
        public void onSignedOut() {
            maybeShow();
        }
    }

    /**
     * View Holder for {@link SignInPromo}.
     */
    public static class ViewHolder extends StatusCardViewHolder {
        private final int mSeparationSpaceSize;

        public ViewHolder(NewTabPageRecyclerView parent, UiConfig config) {
            super(parent, config);
            mSeparationSpaceSize = parent.getResources().getDimensionPixelSize(
                    R.dimen.ntp_sign_in_promo_margin_top);
        }

        @DrawableRes
        @Override
        protected int selectBackground(boolean hasCardAbove, boolean hasCardBelow) {
            assert !hasCardBelow;
            if (hasCardAbove) return R.drawable.ntp_signin_promo_card_bottom;
            return R.drawable.ntp_signin_promo_card_single;
        }

        @Override
        public void updateLayoutParams() {
            super.updateLayoutParams();

            if (getAdapterPosition() == RecyclerView.NO_POSITION) return;

            int precedingPosition = getAdapterPosition() - 1;
            if (precedingPosition < 0) return; // Invalid adapter position, just do nothing.

            @ItemViewType
            int precedingCardType =
                    getRecyclerView().getAdapter().getItemViewType(precedingPosition);

            // The sign in promo should stick to the articles of the preceding section, but have
            // some space otherwise.
            if (precedingCardType != ItemViewType.SNIPPET) {
                getParams().topMargin = mSeparationSpaceSize;
            } else {
                getParams().topMargin = 0;
            }
        }

        @Override
        public boolean isDismissable() {
            return true;
        }
    }

    private void checkIndex(int position) {
        if (position < 0 || position >= getItemCount()) {
            throw new IndexOutOfBoundsException(position + "/" + getItemCount());
        }
    }
}
