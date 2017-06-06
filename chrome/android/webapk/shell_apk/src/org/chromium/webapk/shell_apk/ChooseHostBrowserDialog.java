// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import java.util.List;

/**
 * Shows the dialog to choose a host browser to launch WebAPK. Calls the listener callback when the
 * host browser is chosen.
 */
public class ChooseHostBrowserDialog {
    /**
     * A listener to receive updates when user chooses a host browser for the WebAPK, or dismiss the
     * dialog.
     */
    public interface DialogListener {
        void onHostBrowserSelected(String packageName);
        void onQuit();
    }

    /** Listens to which browser is chosen by the user to launch WebAPK. */
    private DialogListener mListener;

    /**
     * Shows the dialog for choosing a host browser.
     * @param activity The current activity in which to create the dialog.
     * @param url URL of the WebAPK that is shown on the dialog.
     */
    public void show(Activity activity, String url) {
        if (!(activity instanceof DialogListener)) {
            throw new IllegalArgumentException(
                    activity.toString() + " must implement DialogListener");
        }

        mListener = (DialogListener) activity;
        final List<WebApkUtils.BrowserItem> browserItems =
                WebApkUtils.getBrowserInfosForHostBrowserSelection(activity.getPackageManager());

        // The dialog contains:
        // 1) a description of the dialog.
        // 2) a list of browsers for user to choose from.
        View view =
                LayoutInflater.from(activity).inflate(R.layout.choose_host_browser_dialog, null);
        TextView desc = (TextView) view.findViewById(R.id.desc);
        ListView browserList = (ListView) view.findViewById(R.id.browser_list);
        desc.setText(activity.getString(R.string.choose_host_browser, url));
        browserList.setAdapter(new BrowserArrayAdapter(activity, browserItems, url));

        // The context theme wrapper is needed for pre-L.
        AlertDialog.Builder builder = new AlertDialog.Builder(new ContextThemeWrapper(
                activity, android.R.style.Theme_DeviceDefault_Light_Dialog));
        builder.setTitle(activity.getString(R.string.choose_host_browser_dialog_title, url))
                .setView(view)
                .setNegativeButton(R.string.choose_host_browser_dialog_quit,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                mListener.onQuit();
                            }
                        });

        final AlertDialog dialog = builder.create();
        browserList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                WebApkUtils.BrowserItem browserItem = browserItems.get(position);
                if (browserItem.supportsWebApks()) {
                    mListener.onHostBrowserSelected(browserItem.getPackageName());
                    dialog.cancel();
                }
            }
        });

        dialog.show();
    };

    /** Item adaptor for the list of browsers. */
    private static class BrowserArrayAdapter extends ArrayAdapter<WebApkUtils.BrowserItem> {
        private List<WebApkUtils.BrowserItem> mBrowsers;
        private Context mContext;
        private String mUrl;

        public BrowserArrayAdapter(
                Context context, List<WebApkUtils.BrowserItem> browsers, String url) {
            super(context, R.layout.choose_host_browser_dialog_list, browsers);
            mContext = context;
            mBrowsers = browsers;
            mUrl = url;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null) {
                convertView = LayoutInflater.from(mContext).inflate(
                        R.layout.choose_host_browser_dialog_list, null);
            }

            TextView name = (TextView) convertView.findViewById(R.id.browser_name);
            ImageView icon = (ImageView) convertView.findViewById(R.id.browser_icon);
            WebApkUtils.BrowserItem item = mBrowsers.get(position);

            name.setEnabled(item.supportsWebApks());
            if (item.supportsWebApks()) {
                name.setText(item.getApplicationName());
                name.setTextColor(Color.BLACK);
            } else {
                name.setText(mContext.getString(R.string.host_browser_item_not_supporting_webapks,
                        item.getApplicationName(), mUrl));
                name.setSingleLine(false);
                name.setTextColor(Color.LTGRAY);
            }
            icon.setImageDrawable(item.getApplicationIcon());
            icon.setEnabled(item.supportsWebApks());
            return convertView;
        }

        @Override
        public boolean isEnabled(int position) {
            return mBrowsers.get(position).supportsWebApks();
        }
    }
}