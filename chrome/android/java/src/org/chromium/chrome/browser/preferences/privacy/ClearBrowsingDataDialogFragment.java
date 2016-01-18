// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.app.Dialog;
import android.app.DialogFragment;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckedTextView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BrowsingDataType;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.privacy.BrowsingDataCounterBridge.BrowsingDataCounterCallback;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

import java.util.EnumSet;

/**
 * Modal dialog with options for selection the type of browsing data
 * to clear (history, cookies), triggered from a preference.
 */
public class ClearBrowsingDataDialogFragment extends DialogFragment
        implements PrefServiceBridge.OnClearBrowsingDataListener, DialogInterface.OnClickListener {
    /**
     * Represents a single item in the dialog.
     */
    private static class Item implements BrowsingDataCounterCallback {
        private final DialogOption mOption;
        private String mTitle;
        private String mCounterText;
        private boolean mSelected;
        private boolean mEnabled;
        private ArrayAdapter<Item> mParentAdapter;
        private BrowsingDataCounterBridge mCounter;

        public Item(DialogOption option,
                    String title,
                    boolean selected,
                    boolean enabled) {
            super();
            mOption = option;
            mTitle = title;
            mSelected = selected;
            mEnabled = enabled;
            mCounterText = "";
            mParentAdapter = null;
            mCounter = new BrowsingDataCounterBridge(this, mOption.getDataType());
        }

        public void destroy() {
            mCounter.destroy();
        }

        public DialogOption getOption() {
            return mOption;
        }

        @Override
        public String toString() {
            return mTitle;
        }

        public boolean isSelected() {
            return mSelected;
        }

        public void toggle() {
            mSelected = !mSelected;
            PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
                    mOption.getDataType(), mSelected);

            // Counter text is only shown with selected items.
            if (!mSelected) mCounterText = "";
        }

        public boolean isEnabled() {
            return mEnabled;
        }

        public String getCounterText() {
            return mCounterText;
        }

        public void setParentAdapter(ArrayAdapter<Item> adapter) {
            mParentAdapter = adapter;
        }

        @Override
        public void onCounterFinished(String result) {
            mCounterText = result;

            if (mParentAdapter != null) mParentAdapter.notifyDataSetChanged();
        }
    }

    private class ClearBrowsingDataAdapter extends ArrayAdapter<Item> {
        private ClearBrowsingDataAdapter(Item[] items) {
            super(getActivity(), R.layout.clear_browsing_data_dialog_item, R.id.title, items);
            for (Item item : items) item.setParentAdapter(this);
        }

        @Override
        public boolean hasStableIds() {
            return true;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            LinearLayout view = (LinearLayout) super.getView(position, convertView, parent);
            CheckedTextView radioButton = (CheckedTextView) view.findViewById(R.id.title);
            radioButton.setChecked(getItem(position).isSelected());
            radioButton.setEnabled(getItem(position).isEnabled());
            TextView counter = (TextView) view.findViewById(R.id.summary);
            String counterText = getItem(position).getCounterText();
            counter.setText(counterText);

            // Remove the counter if the text is empty, so when the counters experiment is
            // disabled, it doesn't break alignment.
            counter.setVisibility(counterText.isEmpty() ? View.GONE : View.VISIBLE);

            return view;
        }

        public void onClick(int position) {
            if (!getItem(position).isEnabled()) {
                // Currently only the history deletion can be disabled.
                Toast.makeText(getActivity(), R.string.can_not_clear_browsing_history_toast,
                              Toast.LENGTH_SHORT).show();
                return;
            }
            getItem(position).toggle();
            updateButtonState();
            notifyDataSetChanged();
        }
    }

    /** The tag used when showing the clear browsing fragment. */
    public static final String FRAGMENT_TAG = "ClearBrowsingDataDialogFragment";

    /**
     * Enum for Dialog options to be displayed in the dialog.
     */
    public enum DialogOption {
        CLEAR_HISTORY(BrowsingDataType.HISTORY, R.string.clear_history_title),
        CLEAR_CACHE(BrowsingDataType.CACHE, R.string.clear_cache_title),
        CLEAR_COOKIES_AND_SITE_DATA(BrowsingDataType.COOKIES,
                                    R.string.clear_cookies_and_site_data_title),
        CLEAR_PASSWORDS(BrowsingDataType.PASSWORDS, R.string.clear_passwords_title),
        CLEAR_FORM_DATA(BrowsingDataType.FORM_DATA, R.string.clear_formdata_title),
        // Clear bookmarks is only used by ClearSyncData dialog.
        CLEAR_BOOKMARKS_DATA(BrowsingDataType.BOOKMARKS, R.string.clear_bookmarks_title);

        private final int mDataType;
        private final int mResourceId;

        private DialogOption(int dataType, int resourceId) {
            mDataType = dataType;
            mResourceId = resourceId;
        }

        public int getDataType() {
            return mDataType;
        }

        /**
         * @return resource id of the Dialog option.
         */
        public int getResourceId() {
            return mResourceId;
        }
    }

    private AlertDialog mDialog;
    private ProgressDialog mProgressDialog;
    private ClearBrowsingDataAdapter mAdapter;
    private boolean mCanDeleteBrowsingHistory;
    private Item[] mItems;

    protected final EnumSet<DialogOption> getSelectedOptions() {
        EnumSet<DialogOption> selected = EnumSet.noneOf(DialogOption.class);
        for (Item item : mItems) {
            if (item.isSelected()) selected.add(item.getOption());
        }
        return selected;
    }

    protected final void clearBrowsingData() {
        EnumSet<DialogOption> options = getSelectedOptions();
        int[] dataTypes = new int[options.size()];

        int i = 0;
        for (DialogOption option : options) {
            dataTypes[i] = option.getDataType();
            ++i;
        }

        PrefServiceBridge.getInstance().clearBrowsingData(this, dataTypes);
    }

    protected void dismissProgressDialog() {
        android.util.Log.i(FRAGMENT_TAG, "in dismissProgressDialog");
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            android.util.Log.i(FRAGMENT_TAG, "progress dialog dismissed");
            mProgressDialog.dismiss();
        }
        mProgressDialog = null;
    }

    /**
     * Returns the Array of dialog options. Options are displayed in the same
     * order as they appear in the array.
     */
    protected DialogOption[] getDialogOptions() {
        return new DialogOption[] {
            DialogOption.CLEAR_HISTORY,
            DialogOption.CLEAR_CACHE,
            DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
            DialogOption.CLEAR_PASSWORDS,
            DialogOption.CLEAR_FORM_DATA};
    }

    /**
     * Decides whether a given dialog option should be selected when the dialog is initialized.
     * @param option The option in question.
     * @return boolean Whether the given option should be preselected.
     */
    protected boolean isOptionSelectedByDefault(DialogOption option) {
        return PrefServiceBridge.getInstance().getBrowsingDataDeletionPreference(
            option.getDataType());
    }

    // Called when "clear browsing data" completes.
    // Implements the ChromePreferences.OnClearBrowsingDataListener interface.
    @Override
    public void onBrowsingDataCleared() {
        dismissProgressDialog();
    }

    @Override
    public void onClick(DialogInterface dialog, int whichButton) {
        if (whichButton == AlertDialog.BUTTON_POSITIVE) {
            dismissProgressDialog();
            onOptionSelected();
        }
    }

    /**
     * Disable the "Clear" button if none of the options are selected. Otherwise, enable it.
     */
    private void updateButtonState() {
        Button clearButton = mDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        if (clearButton == null) return;
        boolean isEnabled = !getSelectedOptions().isEmpty();
        clearButton.setEnabled(isEnabled);

        // Work around a bug in the app compat library where disabled buttons in alert dialogs
        // don't look disabled on pre-L devices. See: http://crbug.com/550784
        // TODO(newt): remove this workaround when the app compat library is fixed (b/26017217)
        clearButton.setTextColor(isEnabled ? 0xFF4285F4 : 0x335A5A5A);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        mCanDeleteBrowsingHistory = PrefServiceBridge.getInstance().canDeleteBrowsingHistory();

        DialogOption[] options = getDialogOptions();
        mItems = new Item[options.length];
        Resources resources = getResources();
        for (int i = 0; i < options.length; i++) {
            // It is possible to disable the deletion of browsing history.
            boolean enabled = options[i] != DialogOption.CLEAR_HISTORY || mCanDeleteBrowsingHistory;

            mItems[i] = new Item(
                options[i],
                resources.getString(options[i].getResourceId()),
                isOptionSelectedByDefault(options[i]),
                enabled);
        }
        mAdapter = new ClearBrowsingDataAdapter(mItems);
        final AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.AlertDialogTheme)
                        .setTitle(R.string.clear_browsing_data_title)
                        .setPositiveButton(R.string.clear_data_delete, this)
                        .setNegativeButton(R.string.cancel, this)
                        .setAdapter(mAdapter, null);  // OnClickListener is registered manually.

        if (ChromeSigninController.get(getActivity()).isSignedIn()) {
            final String message = getString(R.string.clear_cookies_no_sign_out_summary);
            final SpannableString messageWithLink = SpanApplier.applySpans(message,
                    new SpanApplier.SpanInfo("<link>", "</link>", new ClickableSpan() {
                        @Override
                        public void onClick(View widget) {
                            dismiss();
                            Preferences prefActivity = (Preferences) getActivity();
                            prefActivity.startFragment(AccountManagementFragment.class.getName(),
                                    null);
                        }

                        // Change link formatting to use no underline
                        @Override
                        public void updateDrawState(TextPaint textPaint) {
                            textPaint.setColor(textPaint.linkColor);
                            textPaint.setUnderlineText(false);
                        }
                    }));

            View view = getActivity().getLayoutInflater().inflate(
                    R.layout.single_line_bottom_text_dialog, null);
            TextView summaryView = (TextView) view.findViewById(R.id.summary);
            summaryView.setText(messageWithLink);
            summaryView.setMovementMethod(LinkMovementMethod.getInstance());
            builder.setView(view);
        }

        mDialog = builder.create();
        mDialog.getListView().setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                // The present behaviour of AlertDialog is to dismiss after the onClick event.
                // Hence we are manually overriding this outside the builder.
                mAdapter.onClick(position);
            }
        });
        return mDialog;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        // Now that the dialog's view has been created, update the button state.
        updateButtonState();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        for (Item item : mItems) {
            item.destroy();
        }
    }

    /**
     * Called when PositiveButton is clicked for the dialog.
     */
    protected void onOptionSelected() {
        showProgressDialog();
        clearBrowsingData();
    }

    protected final void showProgressDialog() {
        if (getActivity() == null) return;

        android.util.Log.i(FRAGMENT_TAG, "progress dialog shown");
        mProgressDialog = ProgressDialog.show(getActivity(),
                getActivity().getString(R.string.clear_browsing_data_progress_title),
                getActivity().getString(R.string.clear_browsing_data_progress_message), true,
                false);
    }

    @VisibleForTesting
    ProgressDialog getProgressDialog() {
        return mProgressDialog;
    }
}
