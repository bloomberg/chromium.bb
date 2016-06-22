// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.telephony.PhoneNumberFormattingTextWatcher;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.ui.PaymentRequestUI.PaymentRequestObserverForTest;
import org.chromium.chrome.browser.widget.AlwaysDismissedDialog;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * The PaymentRequest editor view. Can be used for editing contact information, shipping address,
 * billing address, and credit cards.
 */
public class EditorView extends AlwaysDismissedDialog {
    /** The indicator for input fields that are required. */
    private static final String REQUIRED_FIELD_INDICATOR = "*";

    private final Context mContext;
    private final PaymentRequestObserverForTest mObserverForTest;
    private final Handler mHandler;
    private final AsyncTask<Void, Void, PhoneNumberFormattingTextWatcher> mPhoneFormatterTask;

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
    }

    /**
     * Displays the editor user interface for the given model.
     *
     * @param editorModel The description of the editor user interface to display.
     */
    public void show(final EditorModel editorModel) {
        final LinearLayout editor = new LinearLayout(mContext);
        editor.setOrientation(LinearLayout.VERTICAL);
        TextView title = new TextView(mContext);
        title.setText(editorModel.getTitle());
        editor.addView(title);

        Button cancel = new Button(mContext);
        cancel.setId(R.id.payments_edit_cancel_button);
        cancel.setText(mContext.getString(R.string.cancel));
        cancel.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                editorModel.cancel();
                dismiss();
            }
        });

        final Button done = new Button(mContext);
        done.setId(R.id.payments_edit_done_button);
        done.setText(mContext.getString(R.string.done));
        done.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                final List<AutoCompleteTextView> invalidViews =
                        getViewsWithInvalidInformation(editor);
                if (invalidViews.isEmpty()) {
                    editorModel.done();
                    dismiss();
                } else {
                    if (!invalidViews.contains(getCurrentFocus())) {
                        focusInputField(invalidViews.get(0));
                    }
                    ColorFilter errorMessageColorFilter = new PorterDuffColorFilter(
                            ApiCompatibilityUtils.getColor(
                                    mContext.getResources(), R.color.input_underline_error_color),
                            PorterDuff.Mode.SRC_IN);
                    for (int i = 0; i < invalidViews.size(); i++) {
                        AutoCompleteTextView invalid = invalidViews.get(i);
                        invalid.setError(((EditorFieldModel) invalid.getTag()).getErrorMessage());
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                            invalid.getBackground().mutate().setColorFilter(
                                    errorMessageColorFilter);
                        }
                    }
                    if (mObserverForTest != null) {
                        mObserverForTest.onPaymentRequestEditorValidationError();
                    }
                }
            }
        });

        TextView.OnEditorActionListener advanceOrSubmitListener =
                new TextView.OnEditorActionListener() {
                    @Override
                    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                        if (actionId == EditorInfo.IME_ACTION_DONE) {
                            done.performClick();
                            return true;
                        } else if (actionId == EditorInfo.IME_ACTION_NEXT) {
                            View next = v.focusSearch(View.FOCUS_FORWARD);
                            if (next != null && next instanceof AutoCompleteTextView) {
                                next.requestFocus();
                                return true;
                            }
                        }
                        return false;
                    }
                };

        AutoCompleteTextView maybePhoneInput = null;
        for (int i = 0; i < editorModel.getFields().size(); i++) {
            final AutoCompleteTextView input = new AutoCompleteTextView(mContext);
            final EditorFieldModel fieldModel = editorModel.getFields().get(i);
            input.setTag(fieldModel);
            input.setHint(fieldModel.getLabel() + REQUIRED_FIELD_INDICATOR);
            input.setText(fieldModel.getValue());
            input.setOnEditorActionListener(advanceOrSubmitListener);

            input.addTextChangedListener(new TextWatcher() {
                @Override
                public void afterTextChanged(Editable s) {
                    fieldModel.setValue(s.toString());
                    input.getBackground().mutate().setColorFilter(null);
                    input.setError(null);
                }

                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {}
            });

            if (fieldModel.getSuggestions() != null && !fieldModel.getSuggestions().isEmpty()) {
                input.setAdapter(new ArrayAdapter<CharSequence>(mContext,
                        android.R.layout.simple_spinner_dropdown_item,
                        fieldModel.getSuggestions()));
                input.setThreshold(0);
            }

            if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_PHONE) {
                assert maybePhoneInput == null;
                maybePhoneInput = input;
                input.setId(R.id.payments_edit_phone_input);
                input.setInputType(InputType.TYPE_CLASS_PHONE);
                input.addTextChangedListener(getPhoneFormatter());
            } else if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_EMAIL) {
                input.setId(R.id.payments_edit_email_input);
                input.setInputType(
                        InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS);
            }

            editor.addView(input);
        }

        TextView explanation = new TextView(mContext);
        explanation.setText(mContext.getString(R.string.payments_required_field_message));

        editor.addView(explanation);
        editor.addView(cancel);
        editor.addView(done);

        final AutoCompleteTextView phoneInput = maybePhoneInput;
        setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                if (phoneInput != null) phoneInput.removeTextChangedListener(getPhoneFormatter());
                editorModel.cancel();
                if (mObserverForTest != null) mObserverForTest.onPaymentRequestEditorDismissed();
            }
        });
        setContentView(editor);
        show();

        final List<AutoCompleteTextView> invalidViews = getViewsWithInvalidInformation(editor);
        if (!invalidViews.isEmpty()) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    focusInputField(invalidViews.get(0));
                }
            });
        }
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

    private static List<AutoCompleteTextView> getViewsWithInvalidInformation(ViewGroup container) {
        List<AutoCompleteTextView> invalidViews = new ArrayList<>();
        for (int i = 0; i < container.getChildCount(); i++) {
            View view = container.getChildAt(i);
            if (!(view instanceof AutoCompleteTextView)) continue;
            AutoCompleteTextView textView = (AutoCompleteTextView) view;
            if (!((EditorFieldModel) textView.getTag()).isValid()) invalidViews.add(textView);
        }
        return invalidViews;
    }

    private void focusInputField(View view) {
        view.requestFocus();
        InputMethodManager imm =
                (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT);
        view.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        if (mObserverForTest != null) mObserverForTest.onPaymentRequestReadyToEdit();
    }
}
