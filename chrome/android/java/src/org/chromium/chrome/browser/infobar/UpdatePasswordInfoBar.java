// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;

/**
 * The Update Password infobar offers the user the ability to update a password for the site.
 */
public class UpdatePasswordInfoBar
        extends ConfirmInfoBar implements PopupMenu.OnMenuItemClickListener {
    private final long mNativePtr;
    private final String[] mUsernames;
    private final boolean mShowMultipleAccounts;
    private final boolean mIsSmartLockEnabled;
    private final SpannableString mBrandingSpan;
    private int mSelectedUsername;
    private TextView mMessageView;

    @CalledByNative
    private static InfoBar show(long nativePtr, int enumeratedIconId, String[] usernames,
            String primaryButtonText, String secondaryButtonText, String branding,
            boolean showMultipleAccounts, boolean isSmartLockEnabled) {
        return new UpdatePasswordInfoBar(nativePtr, ResourceId.mapToDrawableId(enumeratedIconId),
                usernames, primaryButtonText, secondaryButtonText, branding, showMultipleAccounts,
                isSmartLockEnabled);
    }

    private UpdatePasswordInfoBar(long nativePtr, int iconDrawbleId, String[] usernames,
            String primaryButtonText, String secondaryButtonText, String branding,
            boolean showMultipleAccounts, boolean isSmartLockEnabled) {
        super(iconDrawbleId, null, null, null, primaryButtonText, secondaryButtonText);
        mNativePtr = nativePtr;
        mUsernames = usernames;
        mShowMultipleAccounts = showMultipleAccounts;
        mIsSmartLockEnabled = isSmartLockEnabled;
        mBrandingSpan = new SpannableString(branding);
    }

    private void onUsernameLinkClicked(View v) {
        PopupMenu popup = new PopupMenu(getContext(), v);
        for (int i = 0; i < mUsernames.length; ++i) {
            MenuItem item = popup.getMenu().add(Menu.NONE, i, i, mUsernames[i]);
        }
        popup.setOnMenuItemClickListener(this);
        popup.show();
    }

    /**
     * Returns the infobar message for the case when PasswordManager has a guess which credentials
     * pair should be updated. There are two cases possible: there is only one credential pair that
     * should be updated and there several guesses available. In the first case user is presented
     * with the username of the credentials that should be updated in the second case user is able
     * to choose which of the available credentials to update.
     * It also remembered the choice made.
     */
    private CharSequence createUsernameMessageText(int selectedUsername) {
        CharSequence message;
        mSelectedUsername = selectedUsername;
        final String template = getContext().getString(R.string.update_password_for_account);
        final String usernameToDisplay = mUsernames[mSelectedUsername];
        if (mShowMultipleAccounts) {
            final String downPointingArrow = " \u25BE";
            SpannableString usernameSelector =
                    new SpannableString(usernameToDisplay + downPointingArrow);
            usernameSelector.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    onUsernameLinkClicked(view);
                }
            }, 0, usernameSelector.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
            message = TextUtils.expandTemplate(template, mBrandingSpan, usernameSelector);
        } else {
            message = TextUtils.expandTemplate(template, mBrandingSpan, usernameToDisplay);
        }
        return message;
    }

    /**
     * If user have chosen different than was shown username to update, then we need to change the
     * infobar message in order to reflect this choice.
     */
    public boolean onMenuItemClick(MenuItem item) {
        mMessageView.setText(
                createUsernameMessageText(item.getItemId()), TextView.BufferType.SPANNABLE);
        return false;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        CharSequence message;
        if (mIsSmartLockEnabled) {
            mBrandingSpan.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    onLinkClicked();
                }
            }, 0, mBrandingSpan.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        if (mShowMultipleAccounts || mUsernames.length != 0) {
            message = createUsernameMessageText(0);
        } else {
            String template = getContext().getString(R.string.update_password);
            message = TextUtils.expandTemplate(template, mBrandingSpan);
        }

        mMessageView = layout.getMessageTextView();
        mMessageView.setText(message, TextView.BufferType.SPANNABLE);
    }

    @CalledByNative
    private int getSelectedUsername() {
        return mSelectedUsername;
    }
}
