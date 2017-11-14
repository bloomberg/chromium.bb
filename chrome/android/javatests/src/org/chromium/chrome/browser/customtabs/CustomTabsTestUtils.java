// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Process;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsSession;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;

import java.util.concurrent.TimeoutException;

/**
 * Utility class that contains convenience calls related with custom tabs testing.
 */
public class CustomTabsTestUtils {

    /**
     * Creates the simplest intent that is sufficient to let {@link ChromeLauncherActivity} launch
     * the {@link CustomTabActivity}.
     */
    public static Intent createMinimalCustomTabIntent(
            Context context, String url) {
        CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder(
                CustomTabsSession.createDummySessionForTesting(
                        new ComponentName(context, ChromeLauncherActivity.class)));
        CustomTabsIntent customTabsIntent = builder.build();
        Intent intent = customTabsIntent.intent;
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    public static CustomTabsConnection setUpConnection() {
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.resetThrottling(Process.myUid());
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

    /** Calls warmup() and waits for all the tasks to complete. Fails the test otherwise. */
    public static CustomTabsConnection warmUpAndWait() {
        CustomTabsConnection connection = setUpConnection();
        try {
            final CallbackHelper startupCallbackHelper = new CallbackHelper();
            connection.setWarmupCompletedCallbackForTesting(new Runnable() {
                @Override
                public void run() {
                    startupCallbackHelper.notifyCalled();
                }
            });
            Assert.assertTrue(connection.warmup(0));
            startupCallbackHelper.waitForCallback(0);
        } catch (TimeoutException | InterruptedException e) {
            Assert.fail();
        } finally {
            connection.setWarmupCompletedCallbackForTesting(null);
        }
        return connection;
    }
}
