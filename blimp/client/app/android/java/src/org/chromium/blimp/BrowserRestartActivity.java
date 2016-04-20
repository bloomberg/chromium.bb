// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;

/**
 * Kills and (optionally) restarts the main Blimp process, then immediately kills itself.
 *
 * Starting this Activity requires passing in the process ID (the Intent should have the value of
 * Process#myPid() as an extra).
 *
 * This Activity runs on a separate process from the main Blimp browser and cannot see the main
 * process' Activities.  It is an workaround for crbug.com/515919 which doesn't use AlarmManager.
 */
public class BrowserRestartActivity extends Activity {
    private static final String EXTRA_MAIN_PID =
            "org.chromium.blimp.browser.BrowserRestartActivity.main_pid";
    private static final String EXTRA_RESTART =
            "org.chromium.blimp.browser.BrowserRestartActivity.restart";

    private static final String TAG = "BrowserRestartActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        destroyProcess(getIntent());
    }

    @Override
    public void onNewIntent(Intent intent) {
        destroyProcess(getIntent());
    }

    private void destroyProcess(Intent intent) {
        // Kill the main Blimp process.
        int mainBrowserPid = intent.getIntExtra(BrowserRestartActivity.EXTRA_MAIN_PID, -1);
        assert mainBrowserPid != -1;
        Process.killProcess(mainBrowserPid);

        // Fire an Intent to restart Blimp.
        boolean restart = intent.getBooleanExtra(BrowserRestartActivity.EXTRA_RESTART, false);
        if (restart) {
            Intent restartIntent = new Intent(Intent.ACTION_MAIN);
            restartIntent.setPackage(getPackageName());
            restartIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(restartIntent);
        }

        // Kill this process.
        finish();
        Process.killProcess(Process.myPid());
    }

    /**
     * Helper method to create an Intent to restart the browser.
     * @param context The current android context
     * @return Intent to be fired
     */
    public static Intent createRestartIntent(Context context) {
        Intent intent = new Intent();
        intent.setClassName(context.getPackageName(), BrowserRestartActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(BrowserRestartActivity.EXTRA_MAIN_PID, Process.myPid());
        intent.putExtra(BrowserRestartActivity.EXTRA_RESTART, true);
        return intent;
    }
}
