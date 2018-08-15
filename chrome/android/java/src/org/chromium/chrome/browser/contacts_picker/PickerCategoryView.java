// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.RelativeLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.ContactsPickerListener;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A class for keeping track of common data associated with showing contact details in
 * the contacts picker, for example the RecyclerView.
 */
public class PickerCategoryView extends RelativeLayout implements View.OnClickListener {
    // Constants for the RoundedIconGenerator.
    private static final int ICON_SIZE_DP = 32;
    private static final int ICON_CORNER_RADIUS_DP = 20;
    private static final int ICON_TEXT_SIZE_DP = 12;

    // The dialog that owns us.
    private ContactsPickerDialog mDialog;

    // The view containing the RecyclerView and the toolbar, etc.
    private SelectableListLayout<ContactDetails> mSelectableListLayout;

    // Our activity.
    private Activity mActivity;

    // The callback to notify the listener of decisions reached in the picker.
    private ContactsPickerListener mListener;

    // The toolbar located at the top of the dialog.
    private ContactsPickerToolbar mToolbar;

    // The RecyclerView showing the images.
    private RecyclerView mRecyclerView;

    // The {@link PickerAdapter} for the RecyclerView.
    private PickerAdapter mPickerAdapter;

    // The layout manager for the RecyclerView.
    private LinearLayoutManager mLayoutManager;

    // A helper class to draw the icon for each contact.
    private RoundedIconGenerator mIconGenerator;

    // The {@link SelectionDelegate} keeping track of which contacts are selected.
    private SelectionDelegate<ContactDetails> mSelectionDelegate;

    // The Done text button that confirms the selection choice.
    private Button mDoneButton;

    // The MIME types requested.
    private List<String> mMimeTypes;

    @SuppressWarnings("unchecked") // mSelectableListLayout
    public PickerCategoryView(Context context) {
        super(context);
        mActivity = (Activity) context;

        mSelectionDelegate = new SelectionDelegate<ContactDetails>();

        Resources resources = getActivity().getResources();
        int iconColor =
                ApiCompatibilityUtils.getColor(resources, R.color.default_favicon_background_color);
        mIconGenerator = new RoundedIconGenerator(resources, ICON_SIZE_DP, ICON_SIZE_DP,
                ICON_CORNER_RADIUS_DP, iconColor, ICON_TEXT_SIZE_DP);

        View root = LayoutInflater.from(context).inflate(R.layout.contacts_picker_dialog, this);
        mSelectableListLayout =
                (SelectableListLayout<ContactDetails>) root.findViewById(R.id.selectable_list);

        mPickerAdapter = new PickerAdapter(this);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(mPickerAdapter);
        mToolbar = (ContactsPickerToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.contacts_picker_toolbar, mSelectionDelegate,
                R.string.contacts_picker_select_contacts, null, 0, 0, R.color.modern_primary_color,
                null, false, false);
        mToolbar.setNavigationOnClickListener(this);

        mDoneButton = (Button) mToolbar.findViewById(R.id.done);
        mDoneButton.setOnClickListener(this);

        mLayoutManager = new LinearLayoutManager(mActivity);
        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.setLayoutManager(mLayoutManager);
    }

    /**
     * Initializes the PickerCategoryView object.
     * @param dialog The dialog showing us.
     * @param listener The listener who should be notified of actions.
     * @param mimeTypes A list of mime types to show in the dialog.
     */
    public void initialize(
            ContactsPickerDialog dialog, ContactsPickerListener listener, List<String> mimeTypes) {
        mDialog = dialog;
        mListener = listener;
        mMimeTypes = new ArrayList<>(mimeTypes);

        mDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                executeAction(ContactsPickerListener.ContactsPickerAction.CANCEL, null);
            }
        });

        mPickerAdapter.notifyDataSetChanged();
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.done) {
            notifyContactsSelected();
        } else {
            executeAction(ContactsPickerListener.ContactsPickerAction.CANCEL, null);
        }
    }

    // Simple accessors:

    public Activity getActivity() {
        return mActivity;
    }

    public SelectionDelegate<ContactDetails> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    public RoundedIconGenerator getIconGenerator() {
        return mIconGenerator;
    }

    /**
     * Notifies any listeners that one or more contacts have been selected.
     */
    private void notifyContactsSelected() {
        List<ContactDetails> selectedFiles = mSelectionDelegate.getSelectedItems();
        Collections.sort(selectedFiles);
        String[] contacts = new String[selectedFiles.size()];
        int i = 0;
        for (ContactDetails contactDetails : selectedFiles) {
            contacts[i++] = contactDetails.getDisplayName();
        }

        executeAction(ContactsPickerListener.ContactsPickerAction.CONTACTS_SELECTED, contacts);
    }

    /**
     * Report back what the user selected in the dialog, report UMA and clean up.
     * @param action The action taken.
     * @param contacts The contacts that were selected (if any).
     */
    private void executeAction(
            ContactsPickerListener.ContactsPickerAction action, String[] contacts) {
        mListener.onContactsPickerUserAction(action, contacts);
        mDialog.dismiss();
        UiUtils.onContactsPickerDismissed();
    }
}
