// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.ICustomTabsCallback;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Utility class that contains convenience calls related with custom tabs testing.
 */
public class CustomTabsTestUtils {
    /** Dummy callback, doing nothing. */
    public static class DummyCallback extends ICustomTabsCallback.Stub {
        @Override
        public void onNavigationEvent(int navigationEvent, Bundle extras) {}

        @Override
        public void extraCallback(String callbackName, Bundle args) {}

        @Override
        public IBinder asBinder() {
            return this;
        }
    }

    /**
     * Creates the simplest intent that is sufficient to let {@link ChromeLauncherActivity} launch
     * the {@link CustomTabActivity}.
     */
    public static Intent createMinimalCustomTabIntent(
            Context context, String url, IBinder session) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(context, ChromeLauncherActivity.class));
        IntentUtils.safePutBinderExtra(intent, CustomTabsIntent.EXTRA_SESSION, session);
        return intent;
    }

    public static CustomTabsConnection setUpConnection(Application application) {
        CustomTabsConnection connection = CustomTabsConnection.getInstance(application);
        connection.resetThrottling(application, Process.myUid());
        return connection;
    }

    public static void cleanupSessions(final CustomTabsConnection connection) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.cleanupAll();
            }
        });
    }
}
