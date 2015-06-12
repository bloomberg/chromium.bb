// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.TransactionTooLargeException;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.Menu;

import com.google.android.apps.chrome.R;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.CompositorChromeActivity;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuParams;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.tab.ChromeTab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * A chrome tab that is only used as a custom tab.
 */
public class CustomTab extends ChromeTab {
    private static class CustomTabObserver extends EmptyTabObserver {
        private CustomTabsConnection mCustomTabsConnection;
        private long mSessionId;

        public CustomTabObserver(CustomTabsConnection customTabsConnection, long sessionId) {
            mCustomTabsConnection = customTabsConnection;
            mSessionId = sessionId;
        }

        @Override
        public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
            mCustomTabsConnection.registerLaunch(mSessionId, params.getUrl());
        }

        @Override
        public void onPageLoadStarted(Tab tab, String url) {
            mCustomTabsConnection.notifyPageLoadStarted(mSessionId, url);
        }

        @Override
        public void onPageLoadFinished(Tab tab) {
            mCustomTabsConnection.notifyPageLoadFinished(mSessionId, tab.getUrl());
        }
    }

    private ExternalNavigationHandler mNavigationHandler;
    private CustomTabNavigationDelegate mNavigationDelegate;
    private TabChromeContextMenuItemDelegate
            mContextMenuDelegate = new TabChromeContextMenuItemDelegate() {
                @Override
                public boolean startDownload(String url, boolean isLink) {
                    // Behave similarly to ChromeTabChromeContextMenuItemDelegate in ChromeTab.
                    return !isLink || !shouldInterceptContextMenuDownload(url);
                }
            };

    /**
     * Construct an CustomTab. It might load a prerendered {@link WebContents} for the URL, if
     * {@link CustomTabsConnectionService} has successfully warmed up for the url.
     */
    public CustomTab(CompositorChromeActivity activity, WindowAndroid windowAndroid,
            long sessionId, String url, int parentTabId) {
        super(Tab.generateValidId(Tab.INVALID_TAB_ID), activity, false, windowAndroid,
                TabLaunchType.FROM_EXTERNAL_APP, parentTabId, null, null);
        CustomTabsConnection customTabsConnection =
                CustomTabsConnection.getInstance(activity.getApplication());
        WebContents webContents = customTabsConnection.takePrerenderedUrl(sessionId, url, null);
        if (webContents == null) {
            webContents = WebContentsFactory.createWebContents(isIncognito(), false);
        }
        initialize(webContents, activity.getTabContentManager(), false);
        getView().requestFocus();
        addObserver(new CustomTabObserver(customTabsConnection, sessionId));
    }

    @Override
    protected InterceptNavigationDelegateImpl createInterceptNavigationDelegate() {
        mNavigationDelegate = new CustomTabNavigationDelegate(mActivity);
        mNavigationHandler = new ExternalNavigationHandler(mNavigationDelegate);
        return new InterceptNavigationDelegateImpl(mNavigationHandler);
    }

    /**
     * @return The {@link ExternalNavigationHandler} in this tab. For test purpose only.
     */
    @VisibleForTesting
    ExternalNavigationHandler getExternalNavigationHandler() {
        return mNavigationHandler;
    }

    /**
     * @return The {@link CustomTabNavigationDelegate} in this tab. For test purpose only.
     */
    @VisibleForTesting
    CustomTabNavigationDelegate getExternalNavigationDelegate() {
        return mNavigationDelegate;
    }

    @Override
    protected ContextMenuPopulator createContextMenuPopulator() {
        return new ChromeContextMenuPopulator(mContextMenuDelegate) {
            @Override
            public void buildContextMenu(ContextMenu menu, Context context,
                    ContextMenuParams params) {
                String linkUrl = params.getLinkUrl();
                if (linkUrl != null) linkUrl = linkUrl.trim();
                if (!TextUtils.isEmpty(linkUrl)) {
                    menu.add(Menu.NONE, org.chromium.chrome.R.id.contextmenu_copy_link_address_text,
                            Menu.NONE, org.chromium.chrome.R.string.contextmenu_copy_link_address);
                }

                String linkText = params.getLinkText();
                if (linkText != null) linkText = linkText.trim();
                if (!TextUtils.isEmpty(linkText)) {
                    menu.add(Menu.NONE, org.chromium.chrome.R.id.contextmenu_copy_link_text,
                            Menu.NONE, org.chromium.chrome.R.string.contextmenu_copy_link_text);
                }
                if (params.isImage()) {
                    menu.add(Menu.NONE, R.id.contextmenu_save_image, Menu.NONE,
                            R.string.contextmenu_save_image);
                    menu.add(Menu.NONE, R.id.contextmenu_open_image, Menu.NONE,
                            R.string.contextmenu_open_image);
                    menu.add(Menu.NONE, R.id.contextmenu_copy_image, Menu.NONE,
                            R.string.contextmenu_copy_image);
                    menu.add(Menu.NONE, R.id.contextmenu_copy_image_url, Menu.NONE,
                            R.string.contextmenu_copy_image_url);
                } else if (UrlUtilities.isDownloadableScheme(params.getLinkUrl())) {
                    // "Save link" is not shown for image.
                    menu.add(Menu.NONE, R.id.contextmenu_save_link_as, Menu.NONE,
                            R.string.contextmenu_save_link);
                }
            }
        };
    }

    /**
     * A custom external navigation delegate that forbids the intent picker from showing up.
     */
    static class CustomTabNavigationDelegate extends ExternalNavigationDelegateImpl {
        private static final String TAG = "cr.customtabs";
        private boolean mHasActivityStarted;

        /**
         * Constructs a new instance of {@link CustomTabNavigationDelegate}.
         */
        public CustomTabNavigationDelegate(Activity activity) {
            super(activity);
        }

        @Override
        public void startActivity(Intent intent) {
            super.startActivity(intent);
            mHasActivityStarted = true;
        }

        @Override
        public boolean startActivityIfNeeded(Intent intent) {
            try {
                if (resolveDefaultHandlerForIntent(intent)) {
                    if (getActivity().startActivityIfNeeded(intent, -1)) {
                        mHasActivityStarted = true;
                        return true;
                    } else {
                        return false;
                    }
                } else {
                    // If we failed to find the default handler of the intent, fall back to chrome.
                    return false;
                }
            } catch (RuntimeException e) {
                logTransactionTooLargeOrRethrow(e, intent);
                return false;
            }
        }

        /**
         * Resolve the default external handler of an intent.
         * @return Whether the default external handler is found: if chrome turns out to be the
         *         default handler, this method will return false.
         */
        private boolean resolveDefaultHandlerForIntent(Intent intent) {
            try {
                ResolveInfo info = getActivity().getPackageManager().resolveActivity(intent, 0);
                if (info != null) {
                    final String chromePackage = getActivity().getPackageName();
                    // If a default handler is found and it is not chrome itself, fire the intent.
                    if (info.match != 0 && !chromePackage.equals(info.activityInfo.packageName)) {
                        return true;
                    }
                }
            } catch (RuntimeException e) {
                logTransactionTooLargeOrRethrow(e, intent);
            }
            return false;
        }

        /**
         * @return Whether an external activity has started to handle a url. For testing only.
         */
        @VisibleForTesting
        public boolean hasExternalActivityStarted() {
            return mHasActivityStarted;
        }

        private static void logTransactionTooLargeOrRethrow(RuntimeException e, Intent intent) {
            // See http://crbug.com/369574.
            if (e.getCause() != null && e.getCause() instanceof TransactionTooLargeException) {
                Log.e(TAG, "Could not resolve Activity for intent " + intent.toString(), e);
            } else {
                throw e;
            }
        }
    }

}
