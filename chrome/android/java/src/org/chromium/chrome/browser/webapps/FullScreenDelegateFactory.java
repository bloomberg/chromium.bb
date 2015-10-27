// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.Menu;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.contextmenu.ContextMenuHelper;
import org.chromium.chrome.browser.contextmenu.ContextMenuParams;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.ui.base.Clipboard;

import java.net.URI;

/**
 * A {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned
 * by a {@link FullScreenActivity}.
 */
public class FullScreenDelegateFactory extends TabDelegateFactory {
    private static class FullScreenTabWebContentsDelegateAndroid
            extends TabWebContentsDelegateAndroid {

        public FullScreenTabWebContentsDelegateAndroid(Tab tab, ChromeActivity activity) {
            super(tab, activity);
        }

        @Override
        public void activateContents() {
            if (!(mActivity instanceof WebappActivity)) return;

            WebappInfo webappInfo = ((WebappActivity) mActivity).getWebappInfo();
            String url = webappInfo.uri().toString();

            // Create an Intent that will be fired toward the WebappLauncherActivity, which in turn
            // will fire an Intent to launch the correct WebappActivity.  On L+ this could probably
            // be changed to call AppTask.moveToFront(), but for backwards compatibility we relaunch
            // it the hard way.
            Intent intent = new Intent();
            intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
            intent.setPackage(mActivity.getPackageName());
            webappInfo.setWebappIntentExtras(intent);

            intent.putExtra(ShortcutHelper.EXTRA_MAC, ShortcutHelper.getEncodedMac(mActivity, url));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mActivity.getApplicationContext().startActivity(intent);
        }
    }

    @Override
    public FullScreenTabWebContentsDelegateAndroid createWebContentsDelegate(
            Tab tab, ChromeActivity activity) {
        return new FullScreenTabWebContentsDelegateAndroid(tab, activity);
    }

    @Override
    public ContextMenuPopulator createContextMenuPopulator(Tab tab, final ChromeActivity activity) {
        return new ContextMenuPopulator() {
            private final Context mContext = activity.getApplicationContext();
            private final Clipboard mClipboard = new Clipboard(activity);

            @Override
            public boolean shouldShowContextMenu(ContextMenuParams params) {
                // TODO(yusufo) : This should exactly replicate ChromeContextMenuPopulator.
                return params != null && (params.isAnchor()
                        || params.isImage() || params.isVideo());
            }

            @Override
            public boolean onItemSelected(ContextMenuHelper helper, ContextMenuParams params,
                    int itemId) {
                if (itemId == org.chromium.chrome.R.id.contextmenu_copy_link_address) {
                    String url = params.getUnfilteredLinkUrl();
                    mClipboard.setText(url, url);
                    return true;
                } else if (itemId == org.chromium.chrome.R.id.contextmenu_copy_link_text) {
                    String text = params.getLinkText();
                    mClipboard.setText(text, text);
                    return true;
                } else if (itemId == R.id.menu_id_open_in_chrome) {
                    // TODO(dfalcantara): Merge into the TabDelegate. (https://crbug.com/451453)
                    Intent chromeIntent =
                            new Intent(Intent.ACTION_VIEW, Uri.parse(params.getLinkUrl()));
                    chromeIntent.setPackage(mContext.getPackageName());
                    chromeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

                    boolean activityStarted = false;
                    if (params.getPageUrl() != null) {
                        try {
                            URI pageUri = URI.create(params.getPageUrl());
                            if (UrlUtilities.isInternalScheme(pageUri)) {
                                IntentHandler.startChromeLauncherActivityForTrustedIntent(
                                        chromeIntent, mContext);
                                activityStarted = true;
                            }
                        } catch (IllegalArgumentException ex) {
                            // Ignore the exception for creating the URI and launch the intent
                            // without the trusted intent extras.
                        }
                    }

                    if (!activityStarted) {
                        mContext.startActivity(chromeIntent);
                        activityStarted = true;
                    }
                    return true;
                }

                return false;
            }

            @Override
            public void buildContextMenu(ContextMenu menu, Context context,
                    ContextMenuParams params) {
                menu.add(Menu.NONE, org.chromium.chrome.R.id.contextmenu_copy_link_address,
                        Menu.NONE, org.chromium.chrome.R.string.contextmenu_copy_link_address);

                String linkText = params.getLinkText();
                if (linkText != null) linkText = linkText.trim();

                if (!TextUtils.isEmpty(linkText)) {
                    menu.add(Menu.NONE, org.chromium.chrome.R.id.contextmenu_copy_link_text,
                            Menu.NONE, org.chromium.chrome.R.string.contextmenu_copy_link_text);
                }

                menu.add(Menu.NONE, R.id.menu_id_open_in_chrome, Menu.NONE,
                        R.string.menu_open_in_chrome);
            }
        };
    }
}
