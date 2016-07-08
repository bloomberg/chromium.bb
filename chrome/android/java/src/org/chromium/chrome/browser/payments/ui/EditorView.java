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
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.telephony.PhoneNumberFormattingTextWatcher;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.EditorInfo;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
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

    /** Help page that the user is directed to when asking for help. */
    private static final String HELP_URL = "https://support.google.com/chrome/answer/142893?hl=en";

    private final Context mContext;
    private final PaymentRequestObserverForTest mObserverForTest;
    private final Handler mHandler;
    private final AsyncTask<Void, Void, PhoneNumberFormattingTextWatcher> mPhoneFormatterTask;
    private final TextView.OnEditorActionListener mEditorActionListener;
    private final int mHalfRowMargin;
    private final List<EditorTextField> mEditorTextFields;

    private ViewGroup mLayout;
    private EditorModel mEditorModel;
    private Button mDoneButton;
    private ViewGroup mDataView;
    private View mFooter;
    @Nullable private AutoCompleteTextView mPhoneInput;

    /**
     * Builds the editor view.
     *
     * @param activity        The activity on top of which the UI should be displayed.
     * @param observerForTest Optional event observer for testing.
     */
    public EditorView(Activity activity, PaymentRequestObserverForTest observerForTest) {
        super(activity, R.style.FullscreenWhiteDialog);
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

        mHalfRowMargin = activity.getResources().getDimensionPixelSize(
                R.dimen.payments_section_large_spacing);
        mEditorTextFields = new ArrayList<>();
    }

    /** Launches the Autofill help page on top of the current Context. */
    public static void launchAutofillHelpPage(Context context) {
        EmbedContentViewActivity.show(
                context, context.getString(R.string.help), HELP_URL);
    }

    /**
     * Prepares the toolbar for use.
     *
     * Many of the things that would ideally be set as attributes don't work and need to be set
     * programmatically.  This is likely due to how we compile the support libraries.
     */
    private void prepareToolbar() {
        EditorDialogToolbar toolbar = (EditorDialogToolbar) mLayout.findViewById(R.id.action_bar);
        toolbar.setTitle(mEditorModel.getTitle());
        toolbar.setTitleTextColor(Color.WHITE);
        toolbar.setShowDeleteMenuItem(false);

        // Show the help article when the user asks.
        toolbar.setOnMenuItemClickListener(new OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                launchAutofillHelpPage(mContext);
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
        final List<EditorTextField> invalidViews = getViewsWithInvalidInformation();

        // Focus the first field that's invalid.
        if (!invalidViews.isEmpty() && !invalidViews.contains(getCurrentFocus())) {
            focusInputField(invalidViews.get(0));
        }

        // Iterate over all the fields to update what errors are displayed, which is necessary to
        // to clear existing errors on any newly valid fields.
        for (int i = 0; i < mEditorTextFields.size(); i++) {
            EditorTextField fieldView = mEditorTextFields.get(i);
            fieldView.updateDisplayedError(invalidViews.contains(fieldView));
        }
        return invalidViews.isEmpty();
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
        mDoneButton = (Button) mLayout.findViewById(R.id.button_primary);
        mDoneButton.setId(R.id.payments_edit_done_button);
        mDoneButton.setOnClickListener(this);

        Button cancelButton = (Button) mLayout.findViewById(R.id.button_secondary);
        cancelButton.setId(R.id.payments_edit_cancel_button);
        cancelButton.setOnClickListener(this);

        DualControlLayout buttonBar = (DualControlLayout) mLayout.findViewById(R.id.button_bar);
        buttonBar.setAlignment(DualControlLayout.ALIGN_END);
    }

    /**
     * Create the visual representation of the EditorModel.
     *
     * This would be more optimal as a RelativeLayout, but because it's dynamically generated, it's
     * much more human-parsable with inefficient LinearLayouts for half-width controls sharing rows.
     */
    private void prepareEditor() {
        removeTextChangedListenerFromPhoneInputField();

        // Ensure the layout is empty.
        mDataView = (ViewGroup) mLayout.findViewById(R.id.contents);
        mDataView.removeAllViews();
        mEditorTextFields.clear();

        // Add Views for each of the {@link EditorFields}.
        for (int i = 0; i < mEditorModel.getFields().size(); i++) {
            EditorFieldModel fieldModel = mEditorModel.getFields().get(i);
            EditorFieldModel nextFieldModel = null;

            boolean isLastField = i == mEditorModel.getFields().size() - 1;
            boolean useFullLine = fieldModel.isFullLine();
            if (!isLastField && !useFullLine) {
                // If the next field isn't full, stretch it out.
                nextFieldModel = mEditorModel.getFields().get(i + 1);
                if (nextFieldModel.isFullLine()) useFullLine = true;
            }

            if (useFullLine) {
                addFieldViewToEditor(mDataView, fieldModel);
            } else {
                // Create a LinearLayout to put it and the next view side by side.
                LinearLayout rowLayout = new LinearLayout(mContext);
                mDataView.addView(rowLayout);

                View firstView = addFieldViewToEditor(rowLayout, fieldModel);
                View lastView = addFieldViewToEditor(rowLayout, nextFieldModel);

                LinearLayout.LayoutParams firstParams =
                        (LinearLayout.LayoutParams) firstView.getLayoutParams();
                LinearLayout.LayoutParams lastParams =
                        (LinearLayout.LayoutParams) lastView.getLayoutParams();

                firstParams.width = 0;
                firstParams.weight = 1;
                firstParams.setMarginEnd(mHalfRowMargin);
                lastParams.width = 0;
                lastParams.weight = 1;
                i = i + 1;
            }
        }

        // Add the footer.
        mDataView.addView(mFooter);
    }

    private View addFieldViewToEditor(ViewGroup parent, EditorFieldModel fieldModel) {
        View childView = null;

        if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN) {
            Runnable prepareEditorRunnable = new Runnable() {
                @Override
                public void run() {
                    // The fields may have changed.
                    prepareEditor();
                    if (mObserverForTest != null) mObserverForTest.onPaymentRequestReadyToEdit();
                }
            };
            EditorDropdownField dropdownView =
                    new EditorDropdownField(mContext, fieldModel, prepareEditorRunnable);

            childView = dropdownView.getLayout();
        } else {
            EditorTextField inputLayout = new EditorTextField(mLayout.getContext(), fieldModel,
                    mEditorActionListener, getPhoneFormatter(), mObserverForTest);
            mEditorTextFields.add(inputLayout);

            final AutoCompleteTextView input = inputLayout.getEditText();
            if (fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_PHONE) {
                assert mPhoneInput == null;
                mPhoneInput = input;
            }

            childView = inputLayout;
        }

        parent.addView(childView);
        return childView;
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

        mFooter = LayoutInflater.from(mContext).inflate(
                R.layout.payment_request_editor_footer, null, false);

        prepareToolbar();
        prepareButtons();
        prepareEditor();
        show();

        // Immediately focus the first invalid field to make it faster to edit.
        final List<EditorTextField> invalidViews = getViewsWithInvalidInformation();
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
        removeTextChangedListenerFromPhoneInputField();
        mEditorModel.cancel();
        if (mObserverForTest != null) mObserverForTest.onPaymentRequestEditorDismissed();
    }

    /** @return All the EditorTextFields that exist in the EditorView. */
    @VisibleForTesting
    public List<EditorTextField> getEditorTextFields() {
        return mEditorTextFields;
    }

    private void removeTextChangedListenerFromPhoneInputField() {
        if (mPhoneInput != null) mPhoneInput.removeTextChangedListener(getPhoneFormatter());
        mPhoneInput = null;
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

    private List<EditorTextField> getViewsWithInvalidInformation() {
        List<EditorTextField> invalidViews = new ArrayList<>();
        for (int i = 0; i < mEditorTextFields.size(); i++) {
            EditorTextField fieldView = mEditorTextFields.get(i);
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
