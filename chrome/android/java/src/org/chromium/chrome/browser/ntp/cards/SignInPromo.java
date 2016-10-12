// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.support.annotation.DrawableRes;
import android.support.annotation.StringRes;
import android.support.v7.widget.RecyclerView;

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
public class SignInPromo implements ItemGroup, StatusCardViewHolder.DataSource {
    private final NewTabPageItem mItem = new Item();
    private final List<NewTabPageItem> mItems = Collections.<NewTabPageItem>singletonList(mItem);
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

    public SignInPromo() {
        mDismissed = ChromePreferenceManager.getInstance(ContextUtils.getApplicationContext())
                             .getNewTabPageSigninPromoDismissed();
        mVisible = !SigninManager.get(ContextUtils.getApplicationContext()).isSignedInOnNative();
    }

    @Override
    public List<NewTabPageItem> getItems() {
        return isShown() ? mItems : Collections.<NewTabPageItem>emptyList();
    }

    private class Item implements NewTabPageItem {
        @Override
        public int getType() {
            return NewTabPageItem.VIEW_TYPE_PROMO;
        }

        @Override
        public void onBindViewHolder(NewTabPageViewHolder holder) {
            assert holder instanceof ViewHolder;
            ((ViewHolder) holder).onBindViewHolder(SignInPromo.this);
        }
    }

    @Override
    public void performAction(Context context) {
        AccountSigninActivity.startIfAllowed(context, SigninAccessPoint.NTP_CONTENT_SUGGESTIONS);
    }

    /** Sets the {@link Observer} that will be notified when the visibility of the item changes. */
    public void setObserver(Observer changeObserver) {
        assert mChangeObserver == null;
        this.mChangeObserver = changeObserver;
    }

    @Override
    @StringRes
    public int getHeader() {
        return R.string.snippets_disabled_generic_prompt;
    }

    @Override
    @StringRes
    public int getDescription() {
        return R.string.snippets_disabled_signed_out_instructions;
    }

    @Override
    @StringRes
    public int getActionLabel() {
        return R.string.sign_in_button;
    }

    public boolean isShown() {
        return !mDismissed && mVisible;
    }

    /** Attempts to show the sign in promo. If the user dismissed it before, it will not be shown.*/
    public void maybeShow() {
        if (mVisible) return;
        mVisible = true;

        if (mDismissed) return;

        RecordUserAction.record("Signin_Impression_FromNTPContentSuggestions");
        mChangeObserver.onItemRangeInserted(this, 0, 1);
    }

    /** Hides the sign in promo. */
    public void hide() {
        if (!mVisible) return;
        mVisible = false;

        if (mDismissed) return;

        mChangeObserver.onItemRangeRemoved(this, 0, 1);
    }

    /** Hides the sign in promo and sets a preference to make sure it is not shown again. */
    public void dismiss() {
        hide();
        mDismissed = true;
        ChromePreferenceManager.getInstance(ContextUtils.getApplicationContext())
                .setNewTabPageSigninPromoDismissed(true);
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

            @NewTabPageItem.ViewType
            int precedingCardType =
                    getRecyclerView().getAdapter().getItemViewType(getAdapterPosition() - 1);

            // The sign in promo should stick to the articles of the preceding section, but have
            // some space otherwise.
            if (precedingCardType != NewTabPageItem.VIEW_TYPE_SNIPPET) {
                getParams().topMargin = mSeparationSpaceSize;
            } else {
                getParams().topMargin = 0;
            }
        }
    }
}
