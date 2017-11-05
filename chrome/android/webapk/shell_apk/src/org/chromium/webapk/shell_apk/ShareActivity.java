// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * WebAPK's share handler Activity.
 */
public class ShareActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String startUrl = extractShareTarget();
        if (TextUtils.isEmpty(startUrl)) {
            finish();
            return;
        }

        HostBrowserLauncher launcher = new HostBrowserLauncher(
                this, startUrl, WebApkConstants.SHORTCUT_SOURCE_SHARE, true /* forceNavigation */);
        launcher.selectHostBrowserAndLaunch(() -> finish());
    }

    private String extractShareTarget() {
        ActivityInfo ai;
        try {
            ai = getPackageManager().getActivityInfo(
                    getComponentName(), PackageManager.GET_META_DATA);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
        if (ai == null || ai.metaData == null) {
            return null;
        }

        String shareTemplate = ai.metaData.getString(WebApkMetaDataKeys.SHARE_TEMPLATE);
        if (TextUtils.isEmpty(shareTemplate)) {
            return null;
        }

        Pattern placeholder = Pattern.compile("\\{.*?\\}");
        Matcher matcher = placeholder.matcher(shareTemplate);
        StringBuffer buffer = new StringBuffer();
        while (matcher.find()) {
            String replacement = null;
            if (matcher.group().equals("{text}")) {
                replacement = getEncodedUrlFromIntent(Intent.EXTRA_TEXT);
            } else if (matcher.group().equals("{title}")) {
                replacement = getEncodedUrlFromIntent(Intent.EXTRA_SUBJECT);
            }

            matcher.appendReplacement(buffer, (replacement == null) ? "" : replacement);
        }
        matcher.appendTail(buffer);
        return buffer.toString();
    }

    /** Gets value for {@link key} from the intent and encodes the URL. */
    private String getEncodedUrlFromIntent(String key) {
        String value = getIntent().getStringExtra(key);
        if (value == null) {
            return null;
        }

        try {
            return URLEncoder.encode(value, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            // No-op: we'll just treat this as unspecified.
        }
        return null;
    }
}
