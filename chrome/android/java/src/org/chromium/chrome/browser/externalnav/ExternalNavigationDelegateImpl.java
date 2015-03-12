// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.TransactionTooLargeException;
import android.provider.Browser;
import android.text.TextUtils;
import android.util.Log;


import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.Tab;
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
    private final Activity mActivity;

    public ExternalNavigationDelegateImpl(Activity activity) {
        mActivity = activity;
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
            mActivity.startActivity(intent);
        } catch (RuntimeException e) {
            logTransactionTooLargeOrRethrow(e, intent);
        }
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent) {
        try {
            return mActivity.startActivityIfNeeded(intent, -1);
        } catch (RuntimeException e) {
            logTransactionTooLargeOrRethrow(e, intent);
            return false;
        }
    }

    @Override
    public void startIncognitoIntent(final Intent intent) {
        new AlertDialog.Builder(mActivity)
            .setTitle(R.string.external_app_leave_incognito_warning_title)
            .setMessage(R.string.external_app_leave_incognito_warning)
            .setPositiveButton(R.string.ok, new OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        startActivity(intent);
                    }
                })
            .setNegativeButton(R.string.cancel, null)
            .show();
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
