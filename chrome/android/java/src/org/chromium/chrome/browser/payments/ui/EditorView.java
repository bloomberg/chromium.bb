// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.os.AsyncTask;
import android.os.Handler;
import android.support.design.widget.TextInputLayout;
import android.support.v7.widget.Toolbar;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.telephony.PhoneNumberFormattingTextWatcher;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.EditorInfo;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.EmbedContentViewActivity;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.browser.widget.AlwaysDismissedDialog;
import org.chromium.chrome.browser.widget.DualControlLayout;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import javax.annotation.Nullable;

/**
 * The PaymentRequest editor dialog. Can be used for editing contact information, shipping address,
 * billing address, and credit cards.
 */
public class EditorView extends AlwaysDismissedDialog
        implements OnClickListener, DialogInterface.OnDismissListener {
    /** The indicator for input fields that are required. */
    private static final String REQUIRED_FIELD_INDICATOR = "*";

    /** Help page that the user is directed to when asking for help. */
    private static final String HELP_URL = "https://support.google.com/chrome/answer/142893?hl=en";

    /** Handles validation and display of one field from the {@link EditorFieldModel}. */
    private final class EditorFieldView extends TextInputLayout {
        private final EditorFieldModel mEditorFieldModel;
        private final AutoCompleteTextView mInput;
        private boolean mHasFocusedAtLeastOnce;

        public EditorFieldView(Context context, final EditorFieldModel fieldModel) {
            super(context);
            mEditorFieldModel = fieldModel;

            // Build up the label.  Required fields are indicated by appending a '*'.
            CharSequence label = fieldModel.getLabel();
            if (fieldModel.isRequired()) label = label + REQUIRED_FIELD_INDICATOR;
            setHint(label);

            // The TextView is a child of this class.  The TextInputLayout manages how it looks.
            mInput = new AutoCompleteTextView(context);
            mInput.setText(fieldModel.getValue());
            mInput.setOnEditorActionListener(mEditorActionListener);
            addView(mInput);

            // Validate the field when the user de-focuses it.
            mInput.setOnFocusChangeListener(new OnFocusChangeListener() {
                @Override
                public void onFocusChange(View v, boolean hasFocus) {
                    if (hasFocus) {
                        mHasFocusedAtLeastOnce = true;
                    } else if (mHasFocusedAtLeastOnce) {
                        // Show no errors until the user has already tried to edit the field once.
                        updateDisplayedError(!mEditorFieldModel.isValid());
                    }
                }
            });

            // Update the model as the user edits the field.
            mInput.addTextChangedListener(new TextWatcher() {
                @Override
                public void afterTextChanged(Editable s) {
                    fieldModel.setValue(s.toString());
                    updateDisplayedError(false);
                }

                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {}
            });

            // Display any autofill suggestions.
            if (fieldModel.getSuggestions() != null && !fieldModel.getSuggestions().isEmpty()) {
                mInput.setAdapter(new ArrayAdapter<CharSequence>(mContext,
                        android.R.layout.simple_spinner_dropdown_item,
                        fieldModel.getSuggestions()));
                mInput.setThreshold(0);
            }

            if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_PHONE) {
                mInput.setId(R.id.payments_edit_phone_input);
                mInput.setInputType(InputType.TYPE_CLASS_PHONE);
                mInput.addTextChangedListener(getPhoneFormatter());
            } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_EMAIL) {
                mInput.setId(R.id.payments_edit_email_input);
                mInput.setInputType(
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
            }
        }

        /** @return The EditorFieldModel that the TextView represents. */
        public EditorFieldModel getFieldModel() {
            return mEditorFieldModel;
        }

        /** @return The TextView for the field. */
        public AutoCompleteTextView getTextView() {
            return mInput;
        }

        /**
         * Updates the error display.
         *
         * @param showError If true, displays the error message.  If false, clears it.
         */
        public void updateDisplayedError(boolean showError) {
            setError(showError ? mEditorFieldModel.getErrorMessage() : null);
        }
    }

    private final Context mContext;
    private final PaymentRequestObserverForTest mObserverForTest;
    private final Handler mHandler;
    private final AsyncTask<Void, Void, PhoneNumberFormattingTextWatcher> mPhoneFormatterTask;
    private final TextView.OnEditorActionListener mEditorActionListener;

    private ViewGroup mLayout;
    private EditorModel mEditorModel;
    private Button mDoneButton;
    @Nullable private AutoCompleteTextView mPhoneInput;

    /**
     * Builds the editor view.
     *
     * @param activity        The activity on top of which the UI should be displayed.
     * @param observerForTest Optional event observer for testing.
     */
    public EditorView(Activity activity, PaymentRequestObserverForTest observerForTest) {
        super(activity, R.style.DialogWhenLarge);
        mContext = activity;
        mObserverForTest = observerForTest;
        mHandler = new Handler();
        mPhoneFormatterTask = new AsyncTask<Void, Void, PhoneNumberFormattingTextWatcher>() {
            @Override
            protected PhoneNumberFormattingTextWatcher doInBackground(Void... unused) {
                return new PhoneNumberFormattingTextWatcher();
            }
        }.execute();

        mEditorActionListener = new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    mDoneButton.performClick();
                    return true;
                } else if (actionId == EditorInfo.IME_ACTION_NEXT) {
                    View next = v.focusSearch(View.FOCUS_FORWARD);
                    if (next != null && next instanceof AutoCompleteTextView) {
                        focusInputField(next);
                        return true;
                    }
                }
                return false;
            }
        };
    }

    /**
     * Prepares the toolbar for use.
     *
     * Many of the things that would ideally be set as attributes don't work and need to be set
     * programmatically.  This is likely due to how we compile the support libraries.
     */
    private void prepareToolbar() {
        Toolbar toolbar = (Toolbar) mLayout.findViewById(R.id.action_bar);
        toolbar.setTitle(mEditorModel.getTitle());
        toolbar.setTitleTextColor(Color.WHITE);

        // Show the help article when the user asks.
        toolbar.setOnMenuItemClickListener(new OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                EmbedContentViewActivity.show(
                        mContext, mContext.getString(R.string.help), HELP_URL);
                return true;
            }
        });

        // Cancel editing when the user hits the back arrow.
        toolbar.setNavigationContentDescription(R.string.cancel);
        toolbar.setNavigationIcon(R.drawable.ic_arrow_back_white_24dp);
        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                cancelEdit();
            }
        });

        // Make it appear that the toolbar is floating by adding a shadow.
        FadingShadowView shadow = (FadingShadowView) mLayout.findViewById(R.id.shadow);
        shadow.init(ApiCompatibilityUtils.getColor(mContext.getResources(),
                R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);

        // The top shadow is handled by the toolbar, so hide the one used in the field editor.
        // TODO(dfalcantara): Make the toolbar's shadow cover the field editor instead of pushing
        //                    it down.  Maybe use a RelativeLayout with overlapping views?
        FadingEdgeScrollView scrollView =
                (FadingEdgeScrollView) mLayout.findViewById(R.id.scroll_view);
        scrollView.setShadowVisibility(false, true);
    }

    /**
     * Checks if all of the fields in the form are valid and updates the displayed errors.
     *
     * @return Whether all fields contain valid information.
     */
    private boolean validateForm() {
        final List<EditorFieldView> invalidViews = getViewsWithInvalidInformation();
        if (invalidViews.isEmpty()) return true;

        // Focus the first field that's invalid.
        if (!invalidViews.contains(getCurrentFocus())) focusInputField(invalidViews.get(0));

        // Iterate over all the fields to update what errors are displayed, which is necessary to
        // to clear existing errors on any newly valid fields.
        ViewGroup dataView = (ViewGroup) mLayout.findViewById(R.id.contents);
        for (int i = 0; i < dataView.getChildCount(); i++) {
            if (!(dataView.getChildAt(i) instanceof EditorFieldView)) continue;
            EditorFieldView fieldView = (EditorFieldView) dataView.getChildAt(i);
            fieldView.updateDisplayedError(invalidViews.contains(fieldView));
        }

        return false;
    }

    @Override
    public void onClick(View view) {
        if (view.getId() == R.id.payments_edit_done_button) {
            if (validateForm()) {
                mEditorModel.done();
                dismiss();
                return;
            }

            if (mObserverForTest != null) mObserverForTest.onPaymentRequestEditorValidationError();
        } else if (view.getId() == R.id.payments_edit_cancel_button) {
            cancelEdit();
        }
    }

    private void cancelEdit() {
        mEditorModel.cancel();
        dismiss();
    }

    private void prepareButtons() {
        mDoneButton = DualControlLayout.createButtonForLayout(
                mContext, true, mContext.getString(R.string.done), this);
        mDoneButton.setId(R.id.payments_edit_done_button);

        Button cancelButton = DualControlLayout.createButtonForLayout(
                mContext, false, mContext.getString(R.string.cancel), this);
        cancelButton.setId(R.id.payments_edit_cancel_button);

        DualControlLayout buttonBar =
                (DualControlLayout) mLayout.findViewById(R.id.button_bar);
        buttonBar.setAlignment(DualControlLayout.ALIGN_END);
        buttonBar.setStackedMargin(mContext.getResources().getDimensionPixelSize(
                R.dimen.infobar_margin_between_stacked_buttons));
        buttonBar.addView(mDoneButton);
        buttonBar.addView(cancelButton);
    }

    /** Create the visual representation of the EditorModel. */
    private void prepareEditor() {
        ViewGroup dataView = (ViewGroup) mLayout.findViewById(R.id.contents);
        for (int i = 0; i < mEditorModel.getFields().size(); i++) {
            final EditorFieldModel fieldModel = mEditorModel.getFields().get(i);
            EditorFieldView inputLayout = new EditorFieldView(mContext, fieldModel);

            final AutoCompleteTextView input = inputLayout.getTextView();
            if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_PHONE) {
                assert mPhoneInput == null;
                mPhoneInput = input;
            }

            dataView.addView(inputLayout, dataView.getChildCount() - 1);
        }
    }

    /**
     * Displays the editor user interface for the given model.
     *
     * @param editorModel The description of the editor user interface to display.
     */
    public void show(final EditorModel editorModel) {
        setOnDismissListener(this);
        mEditorModel = editorModel;

        mLayout = (LinearLayout) LayoutInflater.from(mContext).inflate(
                R.layout.payment_request_editor, null);
        setContentView(mLayout);
        prepareToolbar();
        prepareButtons();
        prepareEditor();
        show();

        // Immediately focus the first invalid field to make it faster to edit.
        final List<EditorFieldView> invalidViews = getViewsWithInvalidInformation();
        if (!invalidViews.isEmpty()) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    focusInputField(invalidViews.get(0));
                }
            });
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        if (mPhoneInput != null) mPhoneInput.removeTextChangedListener(getPhoneFormatter());
        mEditorModel.cancel();
        if (mObserverForTest != null) mObserverForTest.onPaymentRequestEditorDismissed();
    }

    /** Immediately returns the phone formatter or null if it has not initialized yet. */
    private PhoneNumberFormattingTextWatcher getPhoneFormatter() {
        try {
            return mPhoneFormatterTask.get(0, TimeUnit.MILLISECONDS);
        } catch (CancellationException | ExecutionException | InterruptedException
                | TimeoutException e) {
            return null;
        }
    }

    private List<EditorFieldView> getViewsWithInvalidInformation() {
        ViewGroup container = (ViewGroup) findViewById(R.id.contents);

        List<EditorFieldView> invalidViews = new ArrayList<>();
        for (int i = 0; i < container.getChildCount(); i++) {
            View layout = container.getChildAt(i);
            if (!(layout instanceof EditorFieldView)) continue;

            EditorFieldView fieldView = (EditorFieldView) layout;
            if (!fieldView.getFieldModel().isValid()) invalidViews.add(fieldView);
        }
        return invalidViews;
    }

    private void focusInputField(View view) {
        view.requestFocus();
        view.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        if (mObserverForTest != null) mObserverForTest.onPaymentRequestReadyToEdit();
    }
}
