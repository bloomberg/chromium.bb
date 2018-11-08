// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.FadingEdgeScrollView;
import org.chromium.ui.UiUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Generic builder for app modal or tab modal alert dialogs.
 */
public class ModalDialogView implements View.OnClickListener {
    /**
     * Interface that controls the actions on the modal dialog.
     */
    public interface Controller {
        /**
         * Handle click event of the buttons on the dialog.
         * @param buttonType The type of the button.
         */
        void onClick(@ButtonType int buttonType);

        /**
         * Handle dismiss event when the dialog is dismissed by actions on the dialog. Note that it
         * can be dangerous to the {@code dismissalCause} for business logic other than metrics
         * recording, unless the dismissal cause is fully controlled by the client (e.g. button
         * clicked), because the dismissal cause can be different values depending on modal dialog
         * type and mode of presentation (e.g. it could be unknown on VR but a specific value on
         * non-VR).
         * @param dismissalCause The reason of the dialog being dismissed.
         * @see DialogDismissalCause
         */
        void onDismiss(@DialogDismissalCause int dismissalCause);
    }

    /**
     * Parameters that can be used to create a new ModalDialogView.
     * Deprecated. Use {@link ModalDialogProperties} instead.
     */
    public static class Params {
        /** Optional: The String to show as the dialog title. */
        public String title;

        /** Optional: The String to show as descriptive text. */
        public String message;

        /**
         * Optional: The customized View to show in the dialog. Note that the message and the
         * custom view cannot be set together.
         */
        public View customView;

        /** Optional: Resource ID of the String to show on the positive button. */
        public @StringRes int positiveButtonTextId;

        /** Optional: Resource ID of the String to show on the negative button. */
        public @StringRes int negativeButtonTextId;

        /**
         * Optional: The String to show on the positive button. Note that String
         * must be null if positiveButtonTextId is not zero.
         */
        public String positiveButtonText;

        /**
         * Optional: The String to show on the negative button.  Note that String
         * must be null if negativeButtonTextId is not zero
         */
        public String negativeButtonText;

        /**
         * Optional: If true the dialog gets cancelled when the user touches outside of the dialog.
         */
        public boolean cancelOnTouchOutside;

        /**
         * Optional: If true, the dialog title is scrollable with the message. Note that the
         * {@link #customView} will have height WRAP_CONTENT if this is set to true.
         */
        public boolean titleScrollable;
    }

    @IntDef({ButtonType.POSITIVE, ButtonType.NEGATIVE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonType {
        int POSITIVE = 0;
        int NEGATIVE = 1;
    }

    private Controller mController;
    private @Nullable Params mParams;

    private final View mDialogView;

    private FadingEdgeScrollView mScrollView;
    private TextView mTitleView;
    private TextView mMessageView;
    private ViewGroup mCustomViewContainer;
    private View mButtonBar;
    private Button mPositiveButton;
    private Button mNegativeButton;
    private boolean mTitleScrollable;

    // TODO(huayinz): Remove this temporary variable once ModalDialogManager takes a model.
    private boolean mCancelOnTouchOutside;

    /**
     * @return The {@link Context} with the modal dialog theme set.
     * Deprecated.
     */
    public static Context getContext() {
        return new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), R.style.ModalDialogTheme);
    }

    /**
     * Temporary constructor before ModalDialogManager takes a model.
     * @param context The {@link Context} from which the dialog view should be inflated.
     * TODO(huayinz): Change this once ModalDialogManager takes a model.
     */
    public ModalDialogView(Context context) {
        mDialogView =
                LayoutInflater.from(new ContextThemeWrapper(context, R.style.ModalDialogTheme))
                        .inflate(R.layout.modal_dialog_view, null);
        initialize();
    }

    /**
     * Constructor for initializing controller and views.
     * Deprecated. Use {@link ModalDialogView(Context)} instead.
     * @param controller The controller for this dialog.
     */
    public ModalDialogView(@NonNull Controller controller, @NonNull Params params) {
        mController = controller;
        mParams = params;
        mDialogView = LayoutInflater.from(getContext()).inflate(R.layout.modal_dialog_view, null);
        initialize();
    }

    private void initialize() {
        mScrollView = mDialogView.findViewById(R.id.modal_dialog_scroll_view);
        mTitleView = mDialogView.findViewById(R.id.title);
        mMessageView = mDialogView.findViewById(R.id.message);
        mCustomViewContainer = mDialogView.findViewById(R.id.custom);
        mButtonBar = mDialogView.findViewById(R.id.button_bar);
        mPositiveButton = mDialogView.findViewById(R.id.positive_button);
        mNegativeButton = mDialogView.findViewById(R.id.negative_button);

        mPositiveButton.setOnClickListener(this);
        mNegativeButton.setOnClickListener(this);
    }

    @Override
    public void onClick(View view) {
        if (view == mPositiveButton) {
            mController.onClick(ButtonType.POSITIVE);
        } else if (view == mNegativeButton) {
            mController.onClick(ButtonType.NEGATIVE);
        }
    }

    /**
     * Prepare the contents before showing the dialog.
     * Deprecated.
     */
    protected void prepareBeforeShow() {
        if (mParams == null) return;

        setTitle(mParams.title);
        setTitleScrollable(mParams.titleScrollable);
        setMessage(mParams.message);
        setCustomView(mParams.customView);

        Resources resources = mDialogView.getResources();
        assert(mParams.positiveButtonTextId == 0 || mParams.positiveButtonText == null);
        if (mParams.positiveButtonTextId != 0) {
            setButtonText(ButtonType.POSITIVE, resources.getString(mParams.positiveButtonTextId));
        } else if (mParams.positiveButtonText != null) {
            setButtonText(ButtonType.POSITIVE, mParams.positiveButtonText);
        }

        assert(mParams.negativeButtonTextId == 0 || mParams.negativeButtonText == null);
        if (mParams.negativeButtonTextId != 0) {
            setButtonText(ButtonType.NEGATIVE, resources.getString(mParams.negativeButtonTextId));
            mNegativeButton.setOnClickListener(this);
        } else if (mParams.negativeButtonText != null) {
            setButtonText(ButtonType.POSITIVE, mParams.negativeButtonText);
        }
    }

    /**
     * @return The content view of this dialog.
     */
    public View getView() {
        return mDialogView;
    }

    /**
     * @return The button that was added to the dialog using {@link Params}.
     * @param button indicates which button should be returned.
     */
    public Button getButton(@ButtonType int button) {
        if (button == ButtonType.POSITIVE) {
            return mPositiveButton;
        } else if (button == ButtonType.NEGATIVE) {
            return mNegativeButton;
        }
        assert false;
        return null;
    }

    /**
     * @return The controller that controls the actions on the dialogs.
     */
    public Controller getController() {
        return mController;
    }

    /**
     * @param controller The {@link Controller} that handles events on user actions.
     */
    void setController(Controller controller) {
        mController = controller;
    }

    /**
     * @return The content description of the dialog view.
     */
    public CharSequence getContentDescription() {
        return mTitleView.getText();
    }

    /** @param title The title of the dialog. */
    public void setTitle(CharSequence title) {
        mTitleView.setText(title);
        updateContentVisibility();
    }

    /** @param titleScrollable Whether the title is scrollable with the message. */
    void setTitleScrollable(boolean titleScrollable) {
        if (mTitleScrollable == titleScrollable) return;

        mTitleScrollable = titleScrollable;
        CharSequence title = mTitleView.getText();

        // Hide the previous title view since the scrollable and non-scrollable title view should
        // not be shown at the same time.
        mTitleView.setVisibility(View.GONE);

        mTitleView = mDialogView.findViewById(titleScrollable ? R.id.scrollable_title : R.id.title);
        setTitle(title);

        LayoutParams layoutParams = (LayoutParams) mCustomViewContainer.getLayoutParams();
        if (titleScrollable) {
            layoutParams.height = LayoutParams.WRAP_CONTENT;
            layoutParams.weight = 0;
            mScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.FADING, FadingEdgeScrollView.EdgeType.FADING);
        } else {
            layoutParams.height = 0;
            layoutParams.weight = 1;
            mScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.NONE, FadingEdgeScrollView.EdgeType.NONE);
        }
        mCustomViewContainer.setLayoutParams(layoutParams);
    }

    /** @param message The message in the dialog content. */
    void setMessage(String message) {
        mMessageView.setText(message);
        updateContentVisibility();
    }

    /** @param view The customized view in the dialog content. */
    void setCustomView(View view) {
        if (mCustomViewContainer.getChildCount() > 0) mCustomViewContainer.removeAllViews();

        if (view != null) {
            UiUtils.removeViewFromParent(view);
            mCustomViewContainer.addView(view);
            mCustomViewContainer.setVisibility(View.VISIBLE);
        } else {
            mCustomViewContainer.setVisibility(View.GONE);
        }
    }

    /**
     * Sets button text for the specified button. If {@code buttonText} is empty or null, the
     * specified button will not be visible.
     * @param buttonType The {@link ButtonType} of the button.
     * @param buttonText The text to be set on the specified button.
     */
    void setButtonText(@ButtonType int buttonType, String buttonText) {
        getButton(buttonType).setText(buttonText);
        updateButtonVisibility();
    }

    /**
     * @param buttonType The {@link ButtonType} of the button.
     * @param enabled Whether the specified button should be enabled.
     */
    void setButtonEnabled(@ButtonType int buttonType, boolean enabled) {
        getButton(buttonType).setEnabled(enabled);
    }

    /**
     * @return Returns true if the dialog is dismissed when the user touches outside of the dialog.
     */
    public boolean getCancelOnTouchOutside() {
        return mCancelOnTouchOutside;
    }

    /**
     * @param cancelOnTouchOutside Whether the dialog can be cancelled on touch outside.
     * TODO(huayinz): Remove this method once ModalDialogManager takes a model.
     */
    void setCancelOnTouchOutside(boolean cancelOnTouchOutside) {
        mCancelOnTouchOutside = cancelOnTouchOutside;
    }

    private void updateContentVisibility() {
        boolean titleVisible = !TextUtils.isEmpty(mTitleView.getText());
        boolean messageVisible = !TextUtils.isEmpty(mMessageView.getText());
        boolean scrollViewVisible = (mTitleScrollable && titleVisible) || messageVisible;

        mTitleView.setVisibility(titleVisible ? View.VISIBLE : View.GONE);
        mMessageView.setVisibility(messageVisible ? View.VISIBLE : View.GONE);
        mScrollView.setVisibility(scrollViewVisible ? View.VISIBLE : View.GONE);
    }

    private void updateButtonVisibility() {
        boolean positiveButtonVisible = !TextUtils.isEmpty(mPositiveButton.getText());
        boolean negativeButtonVisible = !TextUtils.isEmpty(mNegativeButton.getText());
        boolean buttonBarVisible = positiveButtonVisible || negativeButtonVisible;

        mPositiveButton.setVisibility(positiveButtonVisible ? View.VISIBLE : View.GONE);
        mNegativeButton.setVisibility(negativeButtonVisible ? View.VISIBLE : View.GONE);
        mButtonBar.setVisibility(buttonBarVisible ? View.VISIBLE : View.GONE);
    }
}
