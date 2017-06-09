// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

/** Shows the dialog to install a host browser for launching WebAPK. */
public class InstallHostBrowserDialog {
    /**
     * A listener which is notified when user chooses to install the host browser, or dismiss the
     * dialog.
     */
    public interface DialogListener {
        void onConfirmInstall(String packageName);
        void onConfirmQuit();
    }

    /**
     * Shows the dialog to install a host browser.
     * @param context The current context.
     * @param listener The listener for the dialog.
     * @param url URL of the WebAPK for which the dialog is shown.
     * @param hostBrowserPackageName The package name of the host browser.
     * @param hostBrowserApplicationName The application name of the host browser.
     * @param hostBrowserIconId The resource id of the icon of the host browser.
     */
    public static void show(Context context, final DialogListener listener, String url,
            final String hostBrowserPackageName, String hostBrowserApplicationName,
            int hostBrowserIconId) {
        View view = LayoutInflater.from(context).inflate(R.layout.host_browser_list_item, null);
        TextView name = (TextView) view.findViewById(R.id.browser_name);
        ImageView icon = (ImageView) view.findViewById(R.id.browser_icon);

        name.setText(hostBrowserApplicationName);
        name.setTextColor(Color.BLACK);
        icon.setImageResource(hostBrowserIconId);

        // The context theme wrapper is needed for pre-L.
        AlertDialog.Builder builder = new AlertDialog.Builder(
                new ContextThemeWrapper(context, android.R.style.Theme_DeviceDefault_Light_Dialog));
        builder.setTitle(context.getString(R.string.install_host_browser_dialog_title, url))
                .setView(view)
                .setNegativeButton(R.string.choose_host_browser_dialog_quit,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                listener.onConfirmQuit();
                            }
                        })
                .setPositiveButton(R.string.install_host_browser_dialog_install_button,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                listener.onConfirmInstall(hostBrowserPackageName);
                            }
                        });

        AlertDialog dialog = builder.create();
        dialog.show();
    };
}
