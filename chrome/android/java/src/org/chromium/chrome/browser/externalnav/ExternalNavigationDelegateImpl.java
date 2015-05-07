// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.TransactionTooLargeException;
import android.provider.Browser;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;

import java.util.List;

/**
 * The main implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    private static final String TAG = "ExternalNavigationDelegateImpl";
    private static final String PDF_VIEWER = "com.google.android.apps.docs";
    private static final String PDF_MIME = "application/pdf";
    private static final String PDF_SUFFIX = ".pdf";
    private final Activity mActivity;

    public ExternalNavigationDelegateImpl(Activity activity) {
        mActivity = activity;
    }

    /**
     * Retrieve the best activity for the given intent. If a default activity is provided,
     * choose the default one. Otherwise, return the Intent picker if there are more than one
     * capable activities. If the intent is pdf type, return the platform pdf viewer if
     * it is available so user don't need to choose it from Intent picker.
     *
     * @param context Context of the app.
     * @param intent Intent to open.
     * @param allowSelfOpen Whether chrome itself is allowed to open the intent.
     * @return true if the intent can be resolved, or false otherwise.
     */
    public static boolean resolveIntent(Context context, Intent intent, boolean allowSelfOpen) {
        try {
            boolean activityResolved = false;
            ResolveInfo info = context.getPackageManager().resolveActivity(intent, 0);
            if (info != null) {
                final String packageName = context.getPackageName();
                if (info.match != 0) {
                    // There is a default activity for this intent, use that.
                    if (allowSelfOpen || !packageName.equals(info.activityInfo.packageName)) {
                        activityResolved = true;
                    }
                } else {
                    List<ResolveInfo> handlers = context.getPackageManager().queryIntentActivities(
                            intent, PackageManager.MATCH_DEFAULT_ONLY);
                    if (handlers != null && !handlers.isEmpty()) {
                        activityResolved = true;
                        boolean canSelfOpen = false;
                        boolean hasPdfViewer = false;
                        for (ResolveInfo resolveInfo : handlers) {
                            String pName = resolveInfo.activityInfo.packageName;
                            if (packageName.equals(pName)) {
                                canSelfOpen = true;
                            } else if (PDF_VIEWER.equals(pName)) {
                                String filename = intent.getData().getLastPathSegment();
                                if ((filename != null && filename.endsWith(PDF_SUFFIX))
                                        || PDF_MIME.equals(intent.getType())) {
                                    intent.setClassName(pName, resolveInfo.activityInfo.name);
                                    hasPdfViewer = true;
                                    break;
                                }
                            }
                        }
                        if ((canSelfOpen && !allowSelfOpen) && !hasPdfViewer) {
                            activityResolved = false;
                        }
                    }
                }
            }
            return activityResolved;
        } catch (RuntimeException e) {
            logTransactionTooLargeOrRethrow(e, intent);
        }
        return false;
    }

    @Override
    public List<ComponentName> queryIntentActivities(Intent intent) {
        return IntentUtils.getIntentHandlers(mActivity, intent);
    }

    @Override
    public boolean canResolveActivity(Intent intent) {
        try {
            return mActivity.getPackageManager().resolveActivity(intent, 0) != null;
        } catch (RuntimeException e) {
            logTransactionTooLargeOrRethrow(e, intent);
            return false;
        }
    }

    @Override
    public boolean willChromeHandleIntent(Intent intent) {
        try {
            ResolveInfo info = mActivity.getPackageManager().resolveActivity(intent, 0);
            return info != null
                    && info.activityInfo.packageName.equals(mActivity.getPackageName());
        } catch (RuntimeException e) {
            logTransactionTooLargeOrRethrow(e, intent);
            return false;
        }
    }

    @Override
    public boolean isSpecializedHandlerAvailable(Intent intent) {
        try {
            PackageManager pm = mActivity.getPackageManager();
            List<ResolveInfo> handlers = pm.queryIntentActivities(
                    intent,
                    PackageManager.GET_RESOLVED_FILTER);
            if (handlers == null || handlers.size() == 0) {
                return false;
            }
            for (ResolveInfo resolveInfo : handlers) {
                IntentFilter filter = resolveInfo.filter;
                if (filter == null) {
                    // No intent filter matches this intent?
                    // Error on the side of staying in the browser, ignore
                    continue;
                }
                if (filter.countDataAuthorities() == 0 || filter.countDataPaths() == 0) {
                    // Generic handler, skip
                    continue;
                }
                return true;
            }
        } catch (RuntimeException e) {
            logTransactionTooLargeOrRethrow(e, intent);
        }
        return false;
    }

    @Override
    public String getPackageName() {
        return mActivity.getPackageName();
    }

    @Override
    public void startActivity(Intent intent) {
        try {
            resolveIntent(mActivity, intent, true);
            mActivity.startActivity(intent);
        } catch (RuntimeException e) {
            logTransactionTooLargeOrRethrow(e, intent);
        }
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent) {
        try {
            resolveIntent(mActivity, intent, true);
            return mActivity.startActivityIfNeeded(intent, -1);
        } catch (RuntimeException e) {
            logTransactionTooLargeOrRethrow(e, intent);
            return false;
        }
    }

    @Override
    public void startIncognitoIntent(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final Tab tab, final boolean needsToCloseTab) {
        new AlertDialog.Builder(mActivity, R.style.AlertDialogTheme)
            .setTitle(R.string.external_app_leave_incognito_warning_title)
            .setMessage(R.string.external_app_leave_incognito_warning)
            .setPositiveButton(R.string.ok, new OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        startActivity(intent);
                        if (tab != null && !tab.isClosing() && tab.isInitialized()
                                && needsToCloseTab) {
                            tab.getChromeWebContentsDelegateAndroid().closeContents();
                        }
                    }
                })
            .setNegativeButton(R.string.cancel, new OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        loadIntent(intent, referrerUrl, fallbackUrl, tab, needsToCloseTab);
                    }
                })
            .setOnCancelListener(new OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        loadIntent(intent, referrerUrl, fallbackUrl, tab, needsToCloseTab);
                    }
                })
            .show();
    }

    private void loadIntent(Intent intent, String referrerUrl, String fallbackUrl, Tab tab,
            boolean needsToCloseTab) {
        boolean needsToStartIntent = false;
        if (tab == null || tab.isClosing() || !tab.isInitialized()) {
            needsToStartIntent = true;
            needsToCloseTab = false;
        } else if (needsToCloseTab) {
            needsToStartIntent = true;
        }

        String url = fallbackUrl != null ? fallbackUrl : intent.getDataString();
        if (!UrlUtilities.isAcceptedScheme(url)) {
            if (needsToCloseTab) tab.getChromeWebContentsDelegateAndroid().closeContents();
            return;
        }

        if (needsToStartIntent) {
            intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, getPackageName());
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(getPackageName());
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.addTrustedIntentExtras(intent, mActivity);
            startActivity(intent);

            if (needsToCloseTab) tab.getChromeWebContentsDelegateAndroid().closeContents();
            return;
        }

        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL);
        if (!TextUtils.isEmpty(referrerUrl)) {
            Referrer referrer = new Referrer(referrerUrl, 0 /* WebReferrerPolicyAlways */);
            loadUrlParams.setReferrer(referrer);
        }
        tab.loadUrl(loadUrlParams);
    }

    @Override
    public OverrideUrlLoadingResult clobberCurrentTab(
            String url, String referrerUrl, Tab tab) {
        int transitionType = PageTransition.LINK;
        LoadUrlParams loadUrlParams = new LoadUrlParams(url, transitionType);
        if (!TextUtils.isEmpty(referrerUrl)) {
            Referrer referrer = new Referrer(referrerUrl, 0 /* WebReferrerPolicyAlways */);
            loadUrlParams.setReferrer(referrer);
        }
        if (tab != null) {
            tab.loadUrl(loadUrlParams);
            return OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB;
        } else {
            assert false : "clobberCurrentTab was called with an empty tab.";
            Uri uri = Uri.parse(url);
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, getPackageName());
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(getPackageName());
            startActivity(intent);
            return OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT;
        }
    }

    @Override
    public boolean isChromeAppInForeground() {
        return ApplicationStatus.getStateForApplication()
                == ApplicationState.HAS_RUNNING_ACTIVITIES;
    }

    @Override
    public boolean isDocumentMode() {
        return FeatureUtilities.isDocumentMode(mActivity);
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
