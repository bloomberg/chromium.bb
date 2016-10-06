// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.support.annotation.DrawableRes;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.signin.AccountSigninActivity;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninManager;

import java.util.Collections;
import java.util.List;

/**
 * Shows a card prompting the user to sign in. This item is also an {@link ItemGroup}, and calling
 * {@link #hide()} or {@link #maybeShow()} will control its visibility.
 */
public class SigninPromoItem extends StatusItem implements ItemGroup {
    private final List<NewTabPageItem> mItems = Collections.<NewTabPageItem>singletonList(this);
    private Observer mChangeObserver;

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

    public SigninPromoItem() {
        super(org.chromium.chrome.R.string.snippets_disabled_generic_prompt,
                org.chromium.chrome.R.string.snippets_disabled_signed_out_instructions,
                org.chromium.chrome.R.string.sign_in_button);
        mDismissed = ChromePreferenceManager.getInstance(ContextUtils.getApplicationContext())
                             .getNewTabPageSigninPromoDismissed();
        SigninManager signinManager = SigninManager.get(ContextUtils.getApplicationContext());
        mVisible = signinManager.isSignInAllowed() && !signinManager.isSignedInOnNative();
    }

    @Override
    public List<NewTabPageItem> getItems() {
        if (mDismissed) return Collections.emptyList();
        if (!mVisible) return Collections.emptyList();
        return mItems;
    }

    @Override
    public int getType() {
        return NewTabPageItem.VIEW_TYPE_PROMO;
    }

    @Override
    protected void performAction(Context context) {
        AccountSigninActivity.startIfAllowed(context, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS);
    }

    /** Sets the {@link Observer} that will be notified when the visibility of the item changes. */
    public void setObserver(Observer changeObserver) {
        assert mChangeObserver == null;
        this.mChangeObserver = changeObserver;
    }

    /** Attempts to show the sign in promo. If the user dismissed it before, it will not be shown.*/
    public void maybeShow() {
        if (mVisible) return;
        mVisible = true;

        if (mDismissed) return;

        RecordUserAction.record("Signin_Impression_FromNTPContentSuggestions");
        mChangeObserver.notifyItemInserted(this, 0);
    }

    /** Hides the sign in promo. */
    public void hide() {
        if (!mVisible) return;
        mVisible = false;

        if (mDismissed) return;

        mChangeObserver.notifyItemRemoved(this, 0);
    }

    /** Hides the sign in promo and sets a preference to make sure it is not shown again. */
    public void dismiss() {
        hide();
        mDismissed = true;
        ChromePreferenceManager.getInstance(ContextUtils.getApplicationContext())
                .setNewTabPageSigninPromoDismissed(true);
    }

    /**
     * View Holder for {@link SigninPromoItem}.
     */
    public static class ViewHolder extends StatusCardViewHolder {
        public ViewHolder(NewTabPageRecyclerView parent, UiConfig config) {
            super(parent, config);
        }

        @DrawableRes
        @Override
        protected int selectBackground(boolean hasCardAbove, boolean hasCardBelow) {
            assert !hasCardBelow;
            if (hasCardAbove) return R.drawable.ntp_signin_promo_card_bottom;
            return R.drawable.ntp_signin_promo_card_single;
        }
    }
}
