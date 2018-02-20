// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.support.v7.app.AlertDialog;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.banners.AppBannerManager;

/**
 * Displays the "Add to Homescreen" dialog, which contains a (possibly editable) title, icon, and
 * possibly an origin.
 *
 * When show() is invoked, the dialog is shown immediately. A spinner is displayed if any data is
 * not yet fetched, and accepting the dialog is disabled until all data is available and in its
 * place on the screen.
 */
public class AddToHomescreenDialog {
    /**
     * The delegate for which this dialog is displayed. Used by the dialog to indicate when the user
     * accedes to adding to home screen, and when the dialog is dismissed.
     */
    public static interface Delegate {
        /**
         * Called when the user accepts adding the item to the home screen with the provided title.
         */
        void addToHomescreen(String title);

        /**
         * Called when the dialog's lifetime is over and it is dismissed from the screen.
         */
        void onDialogDismissed();
    }

    private AlertDialog mDialog;
    private View mProgressBarView;
    private ImageView mIconView;

    /**
     * The {@mShortcutTitleInput} and the {@mAppLayout} are mutually exclusive, depending on whether
     * the home screen item is a bookmark shortcut or a web app.
     */
    private EditText mShortcutTitleInput;
    private LinearLayout mAppLayout;

    private TextView mAppNameView;
    private TextView mAppOriginView;

    private Delegate mDelegate;

    private boolean mHasIcon;

    public AddToHomescreenDialog(Delegate delegate) {
        mDelegate = delegate;
    }

    @VisibleForTesting
    public AlertDialog getAlertDialogForTesting() {
        return mDialog;
    }

    /**
     * Shows the dialog for adding a shortcut to the home screen.
     * @param activity The current activity in which to create the dialog.
     */
    public void show(final Activity activity) {
        View view = activity.getLayoutInflater().inflate(
                R.layout.add_to_homescreen_dialog, null);
        AlertDialog.Builder builder = new AlertDialog.Builder(activity, R.style.AlertDialogTheme)
                .setTitle(AppBannerManager.getHomescreenLanguageOption())
                .setNegativeButton(R.string.cancel,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int id) {
                                dialog.cancel();
                            }
                        });

        mDialog = builder.create();
        mDialog.getDelegate().setHandleNativeActionModesEnabled(false);
        // On click of the menu item for "add to homescreen", an alert dialog pops asking the user
        // if the title needs to be edited. On click of "Add", shortcut is created. Default
        // title is the title of the page.
        mProgressBarView = view.findViewById(R.id.spinny);
        mIconView = (ImageView) view.findViewById(R.id.icon);
        mShortcutTitleInput = (EditText) view.findViewById(R.id.text);
        mAppLayout = (LinearLayout) view.findViewById(R.id.app_info);

        mAppNameView = (TextView) mAppLayout.findViewById(R.id.name);
        mAppOriginView = (TextView) mAppLayout.findViewById(R.id.origin);

        // The dialog's text field is disabled till the "user title" is fetched,
        mShortcutTitleInput.setVisibility(View.INVISIBLE);

        view.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (mProgressBarView.getMeasuredHeight() == mShortcutTitleInput.getMeasuredHeight()
                        && mShortcutTitleInput.getBackground() != null) {
                    // Force the text field to align better with the icon by accounting for the
                    // padding introduced by the background drawable.
                    mShortcutTitleInput.getLayoutParams().height =
                            mProgressBarView.getMeasuredHeight()
                            + mShortcutTitleInput.getPaddingBottom();
                    v.requestLayout();
                    v.removeOnLayoutChangeListener(this);
                }
            }
        });

        // The "Add" button should be disabled if the dialog's text field is empty.
        mShortcutTitleInput.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void afterTextChanged(Editable editableText) {
                updateAddButtonEnabledState();
            }
        });

        mDialog.setView(view);
        mDialog.setButton(DialogInterface.BUTTON_POSITIVE,
                activity.getResources().getString(R.string.add),
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        mDelegate.addToHomescreen(mShortcutTitleInput.getText().toString());
                    }
                });

        mDialog.setOnShowListener(new DialogInterface.OnShowListener() {
            @Override
            public void onShow(DialogInterface d) {
                updateAddButtonEnabledState();
            }
        });

        mDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                mDialog = null;
                mDelegate.onDialogDismissed();
            }
        });

        mDialog.show();
    }

    /**
     * Called when the home screen icon title (and possibly information from the web manifest) is
     * available. Used for web apps and bookmark shortcuts.
     */
    public void onUserTitleAvailable(String title, String url, boolean isWebapp) {
        // Users may edit the title of bookmark shortcuts, but we respect web app names and do not
        // let users change them.
        if (isWebapp) {
            mShortcutTitleInput.setVisibility(View.GONE);
            mAppNameView.setText(title);
            mAppOriginView.setText(url);
            mAppLayout.setVisibility(View.VISIBLE);
            return;
        }

        mShortcutTitleInput.setText(title);
        mShortcutTitleInput.setVisibility(View.VISIBLE);
    }

    /**
     * Called when the home screen icon is available. Must be called after onUserTitleAvailable().
     * @param icon Icon to use in the launcher.
     */
    public void onIconAvailable(Bitmap icon) {
        mProgressBarView.setVisibility(View.GONE);
        mIconView.setVisibility(View.VISIBLE);
        mIconView.setImageBitmap(icon);

        mHasIcon = true;
        updateAddButtonEnabledState();
    }

    void updateAddButtonEnabledState() {
        boolean enable = mHasIcon
                && (!TextUtils.isEmpty(mShortcutTitleInput.getText())
                           || mAppLayout.getVisibility() == View.VISIBLE);
        mDialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(enable);
    }
}
