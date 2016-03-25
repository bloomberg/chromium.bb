// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.util.HashSet;
import java.util.Set;

/**
 * A general-purpose dialog for presenting a list of things to pick from.
 */
public class ItemChooserDialog {
    /**
     * An interface to implement to get a callback when something has been
     * selected.
     */
    public interface ItemSelectedCallback {
        /**
         * Returns the user selection.
         *
         * @param id The id of the item selected. Blank if the dialog was closed
         * without selecting anything.
         */
        void onItemSelected(String id);
    }

    /**
     * A class representing one data row in the picker.
     */
    public static class ItemChooserRow {
        private final String mKey;
        private final String mDescription;

        public ItemChooserRow(String key, String description) {
            mKey = key;
            mDescription = description;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof ItemChooserRow)) return false;
            if (this == obj) return true;
            ItemChooserRow item = (ItemChooserRow) obj;
            return mKey.equals(item.mKey) && mDescription.equals(item.mDescription);
        }

        @Override
        public int hashCode() {
            return mKey.hashCode() + mDescription.hashCode();
        }
    }

    /**
     * The labels to show in the dialog.
     */
    public static class ItemChooserLabels {
        // The title at the top of the dialog.
        public final CharSequence mTitle;
        // The message to show while there are no results.
        public final CharSequence mSearching;
        // The message to show when no results were produced.
        public final CharSequence mNoneFound;
        // A status message to show above the button row after discovery has
        // stopped and no devices have been found.
        public final CharSequence mStatusIdleNoneFound;
        // A status message to show above the button row after an item has
        // been added and discovery has stopped.
        public final CharSequence mStatusIdleSomeFound;
        // The label for the positive button (e.g. Select/Pair).
        public final CharSequence mPositiveButton;

        public ItemChooserLabels(CharSequence title, CharSequence searching, CharSequence noneFound,
                CharSequence statusIdleNoneFound, CharSequence statusIdleSomeFound,
                CharSequence positiveButton) {
            mTitle = title;
            mSearching = searching;
            mNoneFound = noneFound;
            mStatusIdleNoneFound = statusIdleNoneFound;
            mStatusIdleSomeFound = statusIdleSomeFound;
            mPositiveButton = positiveButton;
        }
    }

    /**
     * The various states the dialog can represent.
     */
    private enum State { STARTING, DISCOVERY_IDLE }

    /**
     * An adapter for keeping track of which items to show in the dialog.
     */
    private class ItemAdapter extends ArrayAdapter<ItemChooserRow>
            implements AdapterView.OnItemClickListener {
        private final LayoutInflater mInflater;

        // The background color of the highlighted item.
        private final int mBackgroundHighlightColor;

        // The color of the non-highlighted text.
        private final int mDefaultTextColor;

        // The zero-based index of the item currently selected in the dialog,
        // or -1 (INVALID_POSITION) if nothing is selected.
        private int mSelectedItem = ListView.INVALID_POSITION;

        // A set of keys that are marked as disabled in the dialog.
        private Set<String> mDisabledEntries = new HashSet<String>();

        public ItemAdapter(Context context, int resource) {
            super(context, resource);

            mInflater = LayoutInflater.from(context);

            mBackgroundHighlightColor = ApiCompatibilityUtils.getColor(getContext().getResources(),
                    R.color.light_active_color);
            mDefaultTextColor = ApiCompatibilityUtils.getColor(getContext().getResources(),
                    R.color.default_text_color);
        }

        @Override
        public void clear() {
            mSelectedItem = ListView.INVALID_POSITION;
            mConfirmButton.setEnabled(false);
            super.clear();
        }

        /**
         * Returns the key of the currently selected item or blank if nothing is
         * selected.
         */
        public String getSelectedItemKey() {
            ItemChooserRow row = getItem(mSelectedItem);
            if (row == null) return "";
            return row.mKey;
        }

        /**
         * Sets whether the itam is enabled. Disabled items are grayed out.
         * @param id The id of the item to affect.
         * @param enabled Whether the item should be enabled or not.
         */
        public void setEnabled(String id, boolean enabled) {
            if (enabled) {
                mDisabledEntries.remove(id);
            } else {
                mDisabledEntries.add(id);
            }
            notifyDataSetChanged();
        }

        @Override
        public boolean isEnabled(int position) {
            ItemChooserRow item = getItem(position);
            return !mDisabledEntries.contains(item.mKey);
        }

        @Override
        public int getViewTypeCount() {
            return 1;
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            TextView view;
            if (convertView instanceof TextView) {
                view = (TextView) convertView;
            } else {
                view = (TextView) mInflater.inflate(
                        R.layout.item_chooser_dialog_row, parent, false);
            }

            // Set highlighting for currently selected item.
            if (position == mSelectedItem) {
                view.setBackgroundColor(mBackgroundHighlightColor);
                view.setTextColor(Color.WHITE);
            } else {
                view.setBackground(null);
                if (!isEnabled(position)) {
                    view.setTextColor(ApiCompatibilityUtils.getColor(getContext().getResources(),
                            R.color.primary_text_disabled_material_light));
                } else {
                    view.setTextColor(mDefaultTextColor);
                }
            }

            ItemChooserRow item = getItem(position);
            view.setText(item.mDescription);
            return view;
        }

        @Override
        public void onItemClick(AdapterView<?> adapter, View view, int position, long id) {
            mSelectedItem = position;
            mConfirmButton.setEnabled(true);
            mItemAdapter.notifyDataSetChanged();
        }
    }

    private Context mContext;

    // The dialog this class encapsulates.
    private Dialog mDialog;

    // The callback to notify when the user selected an item.
    private ItemSelectedCallback mItemSelectedCallback;

    // Individual UI elements.
    private TextViewWithClickableSpans mTitle;
    private TextViewWithClickableSpans mEmptyMessage;
    private ProgressBar mProgressBar;
    private ListView mListView;
    private TextView mStatus;
    private Button mConfirmButton;

    // The labels to display in the dialog.
    private ItemChooserLabels mLabels;

    // The adapter containing the items to show in the dialog.
    private ItemAdapter mItemAdapter;

    // How much of the height of the screen should be taken up by the listview.
    private static final double LISTVIEW_HEIGHT_PERCENT = 0.30;
    // The minimum height of the listview in the dialog (in dp).
    private static final int MIN_HEIGHT_DP = 56;
    // The maximum height of the listview in the dialog (in dp).
    private static final int MAX_HEIGHT_DP = 400;

    /**
     * Creates the ItemChooserPopup and displays it (and starts waiting for data).
     *
     * @param context Context which is used for launching a dialog.
     * @param callback The callback used to communicate back what was selected.
     * @param labels The labels to show in the dialog.
     */
    public ItemChooserDialog(
            Context context, ItemSelectedCallback callback, ItemChooserLabels labels) {
        mContext = context;
        mItemSelectedCallback = callback;
        mLabels = labels;

        LinearLayout dialogContainer = (LinearLayout) LayoutInflater.from(
                mContext).inflate(R.layout.item_chooser_dialog, null);

        mListView = (ListView) dialogContainer.findViewById(R.id.items);
        mProgressBar = (ProgressBar) dialogContainer.findViewById(R.id.progress);
        mStatus = (TextView) dialogContainer.findViewById(R.id.status);
        mTitle = (TextViewWithClickableSpans) dialogContainer.findViewById(
                R.id.dialog_title);
        mEmptyMessage =
                (TextViewWithClickableSpans) dialogContainer.findViewById(R.id.not_found_message);

        mTitle.setText(labels.mTitle);
        mTitle.setMovementMethod(LinkMovementMethod.getInstance());

        mEmptyMessage.setMovementMethod(LinkMovementMethod.getInstance());
        mStatus.setMovementMethod(LinkMovementMethod.getInstance());

        mConfirmButton = (Button) dialogContainer.findViewById(R.id.positive);
        mConfirmButton.setText(labels.mPositiveButton);
        mConfirmButton.setEnabled(false);
        mConfirmButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mItemSelectedCallback.onItemSelected(mItemAdapter.getSelectedItemKey());
                mDialog.setOnDismissListener(null);
                mDialog.dismiss();
            }
        });

        mItemAdapter = new ItemAdapter(mContext, R.layout.item_chooser_dialog_row);
        mListView.setAdapter(mItemAdapter);
        mListView.setEmptyView(mEmptyMessage);
        mListView.setOnItemClickListener(mItemAdapter);
        mListView.setDivider(null);
        setState(State.STARTING);

        // The list is the main element in the dialog and it should grow and
        // shrink according to the size of the screen available (clamped to a
        // min and a max).
        View listViewContainer = dialogContainer.findViewById(R.id.container);
        DisplayMetrics metrics = new DisplayMetrics();
        WindowManager manager = (WindowManager) mContext.getSystemService(
                Context.WINDOW_SERVICE);
        manager.getDefaultDisplay().getMetrics(metrics);

        float density = context.getResources().getDisplayMetrics().density;
        int height = (int) (metrics.heightPixels * LISTVIEW_HEIGHT_PERCENT);
        height = Math.min(height, Math.round(MAX_HEIGHT_DP * density));
        height = Math.max(height, Math.round(MIN_HEIGHT_DP * density));
        listViewContainer.setLayoutParams(
                new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, height));

        showDialogForView(dialogContainer);
    }

    private void showDialogForView(View view) {
        mDialog = new Dialog(mContext);
        mDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        mDialog.addContentView(view,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                                              LinearLayout.LayoutParams.MATCH_PARENT));
        mDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                mItemSelectedCallback.onItemSelected("");
            }
        });

        Window window = mDialog.getWindow();
        if (!DeviceFormFactor.isTablet(mContext)) {
            // On smaller screens, make the dialog fill the width of the screen,
            // and appear at the top.
            window.setBackgroundDrawable(new ColorDrawable(Color.WHITE));
            window.setGravity(Gravity.TOP);
            window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT,
                             ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        mDialog.show();
    }

    public void dismiss() {
        mDialog.dismiss();
    }

    /**
    * Add an item to the end of the list to show in the dialog.
    *
    * @param item The item to be added to the end of the chooser.
    */
    public void addItemToList(ItemChooserRow item) {
        mProgressBar.setVisibility(View.GONE);
        mItemAdapter.add(item);
        setState(State.DISCOVERY_IDLE);
    }

    /**
    * Remove an item that is shown in the dialog.
    *
    * @param item The item to be removed in the chooser.
    */
    public void removeItemFromList(ItemChooserRow item) {
        mItemAdapter.remove(item);
        setState(State.DISCOVERY_IDLE);
    }

    /**
     * Indicates the chooser that no more items will be added.
     */
    public void setIdleState() {
        mProgressBar.setVisibility(View.GONE);
        setState(State.DISCOVERY_IDLE);
    }

    /**
     * Sets whether the item is enabled.
     * @param id The id of the item to affect.
     * @param enabled Whether the item should be enabled or not.
     */
    public void setEnabled(String id, boolean enabled) {
        mItemAdapter.setEnabled(id, enabled);
    }

    /**
     * Clear all items from the dialog.
     */
    public void clear() {
        mItemAdapter.clear();
        setState(State.STARTING);
    }

    /**
     * Shows an error message in the dialog.
     */
    public void setErrorState(SpannableString errorMessage, SpannableString errorStatus) {
        mListView.setVisibility(View.GONE);
        mProgressBar.setVisibility(View.GONE);
        mEmptyMessage.setText(errorMessage);
        mEmptyMessage.setVisibility(View.VISIBLE);
        mStatus.setText(errorStatus);
    }

    private void setState(State state) {
        switch (state) {
            case STARTING:
                mStatus.setText(mLabels.mSearching);
                mListView.setVisibility(View.GONE);
                mProgressBar.setVisibility(View.VISIBLE);
                mEmptyMessage.setVisibility(View.GONE);
                break;
            case DISCOVERY_IDLE:
                boolean showEmptyMessage = mItemAdapter.isEmpty();
                mStatus.setText(showEmptyMessage
                        ? mLabels.mStatusIdleNoneFound : mLabels.mStatusIdleSomeFound);
                mEmptyMessage.setText(mLabels.mNoneFound);
                mEmptyMessage.setVisibility(showEmptyMessage ? View.VISIBLE : View.GONE);
                break;
        }
    }

    /**
     * Returns the dialog associated with this class. For use with tests only.
     */
    @VisibleForTesting
    public Dialog getDialogForTesting() {
        return mDialog;
    }

    /**
     * Returns the ItemAdapter associated with this class. For use with tests only.
     */
    @VisibleForTesting
    public ItemAdapter getItemAdapterForTesting() {
        return mItemAdapter;
    }
}
