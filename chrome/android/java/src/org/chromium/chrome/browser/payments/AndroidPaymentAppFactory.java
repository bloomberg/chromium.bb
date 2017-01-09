// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppCreatedCallback;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppFactoryAddition;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Builds instances of payment apps based on installed third party Android payment apps. */
public class AndroidPaymentAppFactory implements PaymentAppFactoryAddition {
    private static final String ACTION_IS_READY_TO_PAY =
            "org.chromium.intent.action.IS_READY_TO_PAY";
    private static final String METHOD_PREFIX = "https://";

    @Override
    public void create(Context context, WebContents webContents, Set<String> methods,
            PaymentAppCreatedCallback callback) {
        Map<String, AndroidPaymentApp> installedApps = new HashMap<>();
        PackageManager pm = context.getPackageManager();
        Intent payIntent = new Intent(AndroidPaymentApp.ACTION_PAY);

        for (String methodName : methods) {
            if (!methodName.startsWith(METHOD_PREFIX)) continue;

            payIntent.setData(Uri.parse(methodName));
            List<ResolveInfo> matches = pm.queryIntentActivities(payIntent, 0);
            for (int i = 0; i < matches.size(); i++) {
                ResolveInfo match = matches.get(i);
                String packageName = match.activityInfo.packageName;
                AndroidPaymentApp installedApp = installedApps.get(packageName);
                if (installedApp == null) {
                    CharSequence label = match.loadLabel(pm);
                    installedApp =
                            new AndroidPaymentApp(webContents, packageName, match.activityInfo.name,
                                    label == null ? "" : label.toString(), match.loadIcon(pm));
                    callback.onPaymentAppCreated(installedApp);
                    installedApps.put(packageName, installedApp);
                }
                installedApp.addMethodName(methodName);
            }
        }

        List<ResolveInfo> matches =
                pm.queryIntentServices(new Intent(ACTION_IS_READY_TO_PAY), 0);
        for (int i = 0; i < matches.size(); i++) {
            ResolveInfo match = matches.get(i);
            String packageName = match.serviceInfo.packageName;
            AndroidPaymentApp installedApp = installedApps.get(packageName);
            if (installedApp != null) installedApp.setIsReadyToPayService(match.serviceInfo.name);
        }

        callback.onAllPaymentAppsCreated();
    }
}
