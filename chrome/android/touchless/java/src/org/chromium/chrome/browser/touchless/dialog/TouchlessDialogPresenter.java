// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.dialog;

import android.app.Activity;
import android.app.Dialog;
import android.support.v4.view.ViewCompat;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.DialogListItemProperties;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.ListItemType;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.Presenter;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ChromeImageView;

import java.util.ArrayList;

/** A modal dialog presenter that is specific to touchless dialogs. */
public class TouchlessDialogPresenter extends Presenter {
    /** An activity to attach dialogs to. */
    private final Activity mActivity;

    /** The dialog this class abstracts. */
    private Dialog mDialog;

    /** The model change processor for the currently shown dialog. */
    private PropertyModelChangeProcessor<PropertyModel, Pair<ViewGroup, ModelListAdapter>,
            PropertyKey> mModelChangeProcessor;

    public TouchlessDialogPresenter(Activity activity) {
        mActivity = activity;
    }

    @Override
    protected void addDialogView(PropertyModel model) {
        // If the activity's decor view is not attached to window, we don't show the dialog because
        // the window manager might have revoked the window token for this activity. See
        // https://crbug.com/926688.
        Window window = mActivity.getWindow();
        if (window == null || !ViewCompat.isAttachedToWindow(window.getDecorView())) {
            dismissCurrentDialog(DialogDismissalCause.NOT_ATTACHED_TO_WINDOW);
            return;
        }

        mDialog = new Dialog(mActivity, R.style.Theme_Chromium_DialogWhenLarge);
        mDialog.setOnCancelListener(dialogInterface
                -> dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE));
        // Cancel on touch outside should be disabled by default. The ModelChangeProcessor wouldn't
        // notify change if the property is not set during initialization.
        mDialog.setCanceledOnTouchOutside(false);
        ViewGroup dialogView = (ViewGroup) LayoutInflater.from(mDialog.getContext())
                                       .inflate(R.layout.touchless_dialog_view, null);
        ModelListAdapter adapter = new ModelListAdapter(mActivity);
        adapter.registerType(ListItemType.DEFAULT, new ModelListAdapter.ViewBuilder<View>() {
            @Override
            public View buildView() {
                View view = LayoutInflater.from(mActivity).inflate(R.layout.dialog_list_item, null);
                view.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View view, boolean focused) {
                        // TODO(mdjones): Do selected item styling here.
                    }
                });
                return view;
            }
        }, TouchlessDialogPresenter::bindListItem);
        ListView dialogOptions = dialogView.findViewById(R.id.touchless_dialog_option_list);
        dialogOptions.setAdapter(adapter);
        dialogOptions.setItemsCanFocus(true);
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                model, Pair.create(dialogView, adapter), TouchlessDialogPresenter::bind);
        mDialog.setContentView(dialogView);
        mDialog.show();
        dialogView.announceForAccessibility(getContentDescription(model));
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }

        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
    }

    /**
     * Bind a model to a touchless dialog view.
     * @param model The model being bound.
     * @param view The view to apply the model to.
     * @param propertyKey The property that changed.
     */
    private static void bind(
            PropertyModel model, Pair<ViewGroup, ModelListAdapter> view, PropertyKey propertyKey) {
        ViewGroup dialogView = view.first;
        ModelListAdapter optionsAdapter = view.second;
        // TODO(mdjones): If the default buttons are used assert no list items and convert the
        //                buttons to list items.
        if (TouchlessDialogProperties.IS_FULLSCREEN == propertyKey) {
            // TODO(mdjones): Implement fullscreen/non-fullscreen modes.
        } else if (ModalDialogProperties.TITLE_ICON == propertyKey) {
            ChromeImageView imageView = dialogView.findViewById(R.id.touchless_dialog_icon);
            imageView.setImageDrawable(model.get(ModalDialogProperties.TITLE_ICON));
            imageView.setVisibility(View.VISIBLE);
        } else if (ModalDialogProperties.TITLE == propertyKey) {
            TextView textView = dialogView.findViewById(R.id.touchless_dialog_title);
            textView.setText(model.get(ModalDialogProperties.TITLE));
            textView.setVisibility(View.VISIBLE);
        } else if (ModalDialogProperties.MESSAGE == propertyKey) {
            TextView textView = dialogView.findViewById(R.id.touchless_dialog_description);
            textView.setText(model.get(ModalDialogProperties.MESSAGE));
            textView.setVisibility(View.VISIBLE);
        } else if (TouchlessDialogProperties.LIST_MODELS == propertyKey) {
            ListView listView = dialogView.findViewById(R.id.touchless_dialog_option_list);
            PropertyModel[] models = model.get(TouchlessDialogProperties.LIST_MODELS);
            ArrayList<Pair<Integer, PropertyModel>> modelPairs = new ArrayList<>();
            for (int i = 0; i < models.length; i++) modelPairs.add(Pair.create(0, models[i]));
            optionsAdapter.updateModels(modelPairs);
        }
    }

    /**
     * Bind a list model item to its view.
     * @param model The model to bind.
     * @param view The view to be bound to.
     * @param propertyKey The property that changed.
     */
    private static void bindListItem(PropertyModel model, View view, PropertyKey propertyKey) {
        if (DialogListItemProperties.ICON == propertyKey) {
            ChromeImageView imageView = view.findViewById(R.id.dialog_item_icon);
            imageView.setImageDrawable(model.get(DialogListItemProperties.ICON));
        } else if (DialogListItemProperties.TEXT == propertyKey) {
            TextView textView = view.findViewById(R.id.dialog_item_text);
            textView.setText(model.get(DialogListItemProperties.TEXT));
        } else if (DialogListItemProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(DialogListItemProperties.CLICK_LISTENER));
        }
    }
}
