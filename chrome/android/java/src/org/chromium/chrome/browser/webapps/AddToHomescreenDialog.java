// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Handler;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.Tab;

/**
 * Helper class showing the UI regarding Add to Homescreen and delegate the
 * logic to ShortcutHelper.
 */
public class AddToHomescreenDialog {
    private static AlertDialog sCurrentDialog;

    @VisibleForTesting
    public static AlertDialog getCurrentDialogForTest() {
        return sCurrentDialog;
    }

    /**
     * Shows the dialog for adding a shortcut to the home screen.
     * @param activity The current activity in which to create the dialog.
     * @param currentTab The current tab for which the shortcut is being created.
     */
    public static void show(final Activity activity, final Tab currentTab) {
        View view = activity.getLayoutInflater().inflate(
                R.layout.single_line_edit_dialog, null);
        AlertDialog.Builder builder = new AlertDialog.Builder(activity)
                .setTitle(R.string.menu_add_to_homescreen)
                .setNegativeButton(R.string.cancel,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int id) {
                                dialog.cancel();
                            }
                        });

        final AlertDialog dialog = builder.create();
        // On click of the menu item for "add to homescreen", an alert dialog pops asking the user
        // if the title needs to be edited. On click of "Add", shortcut is created. Default
        // title is the title of the page. OK button is disabled if the title text is empty.
        TextView titleLabel = (TextView) view.findViewById(R.id.title);
        final EditText input = (EditText) view.findViewById(R.id.text);
        titleLabel.setText(R.string.add_to_homescreen_title);
        input.setEnabled(false);

        final ShortcutHelper shortcutHelper =
                new ShortcutHelper(activity.getApplicationContext(), currentTab);

        // Initializing the shortcut helper is asynchronous. Until it is
        // initialized, the UI will show a disabled text field and OK buttons.
        // They will be enabled and pre-filled as soon as the onInitialized
        // callback will be run. The user will still be able to cancel the
        // operation.
        shortcutHelper.initialize(new ShortcutHelper.OnInitialized() {
            @Override
            public void onInitialized(String title) {
                dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(true);

                input.setEnabled(true);
                input.setText(title);
            }
        });

        input.addTextChangedListener(new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void afterTextChanged(Editable editableText) {
                if (TextUtils.isEmpty(editableText)) {
                    dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(false);
                } else {
                    dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(true);
                }
            }
        });

        dialog.setView(view);
        dialog.setButton(DialogInterface.BUTTON_POSITIVE,
                activity.getResources().getString(R.string.add),
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        new Handler().postDelayed(new Runnable() {
                            @Override
                            public void run() {
                                shortcutHelper.addShortcut(input.getText().toString());
                                // No need to call tearDown() in that case,
                                // |shortcutHelper| is expected to tear down itself
                                // after that call.
                            }
                        }, 100);
                        // We are adding arbitrary delay here to try and yield for main activity to
                        // be focused before we create shortcut. Yielding helps clear this dialog
                        // box from screenshot that Android's recents activity module shows.
                        // I tried a bunch of solutions suggested in code review, but root cause
                        // seems be that parent view/activity doesn't get chance to redraw itself
                        // completely and Recent Activities seem to have old snapshot. Clean way
                        // would be to let parent activity/view draw itself before we add the
                        // shortcut, but that would require complete redrawing including rendered
                        // page.
                    }
                });

        // The dialog is being cancel by clicking away or clicking the "cancel"
        // button. |shortcutHelper| need to be tear down in that case to release
        // all the objects, including the C++ ShortcutHelper.
        dialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                shortcutHelper.tearDown();
            }
        });

        // The "OK" button should only be shown when |shortcutHelper| is
        // initialized, it should be kept disabled until then.
        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
            @Override
            public void onShow(DialogInterface d) {
                if (!shortcutHelper.isInitialized())
                    dialog.getButton(DialogInterface.BUTTON_POSITIVE).setEnabled(false);
            }
        });

        // We need to keep track of the current dialog for testing purposes.
        sCurrentDialog = dialog;
        dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                sCurrentDialog = null;
            }
        });

        dialog.show();
    }
}
