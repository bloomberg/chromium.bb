// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Process;
import android.os.RemoteException;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.base.test.util.DisabledTest;

import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests to ensure that it is impossible to launch two browser processes within
 * the same application. Chromium is not designed for that, and attempting to do that
 * can cause data files corruption.
 */
public class AwSecondBrowserProcessTest extends AwTestBase {
    private CountDownLatch mSecondBrowserProcessLatch;
    private int mSecondBrowserServicePid;

    @Override
    protected boolean needsBrowserProcessStarted() {
        return false;
    }

    @Override
    protected void tearDown() throws Exception {
        stopSecondBrowserProcess(false);
        super.tearDown();
    }

    /*
     * @LargeTest
     * @Feature({"AndroidWebView"})
     * We can't test that creating second browser
     * process succeeds either, because in debug it will crash due to an assert
     * in the SQL DB code.
     */
    @DisabledTest(message = "crbug.com/582146")
    public void testCreatingSecondBrowserProcessFails() throws Throwable {
        startSecondBrowserProcess();
        assertFalse(tryStartingBrowserProcess());
    }

    /*
     * @LargeTest
     * @Feature({"AndroidWebView"})
     */
    @DisabledTest(message = "crbug.com/582146")
    public void testLockCleanupOnProcessShutdown() throws Throwable {
        startSecondBrowserProcess();
        assertFalse(tryStartingBrowserProcess());
        stopSecondBrowserProcess(true);
        assertTrue(tryStartingBrowserProcess());
    }

    private final ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            Parcel result = Parcel.obtain();
            try {
                assertTrue(service.transact(
                                SecondBrowserProcess.CODE_START, Parcel.obtain(), result, 0));
            } catch (RemoteException e) {
                fail("RemoteException: " + e);
            }
            result.readException();
            mSecondBrowserServicePid = result.readInt();
            assertTrue(mSecondBrowserServicePid > 0);
            mSecondBrowserProcessLatch.countDown();
        }

        @Override
        public void onServiceDisconnected(ComponentName className) {
        }
    };

    private void startSecondBrowserProcess() throws Exception {
        Context context = getActivity();
        Intent intent = new Intent(context, SecondBrowserProcess.class);
        mSecondBrowserProcessLatch = new CountDownLatch(1);
        assertNotNull(context.startService(intent));
        assertTrue(context.bindService(intent, mConnection, 0));
        assertTrue(mSecondBrowserProcessLatch.await(
                        AwTestBase.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        mSecondBrowserProcessLatch = null;
    }

    private void stopSecondBrowserProcess(boolean sync) throws Exception {
        if (mSecondBrowserServicePid <= 0) return;
        assertTrue(isSecondBrowserServiceRunning());
        // Note that using killProcess ensures that the service record gets removed
        // from ActivityManager after the process has actually died. While using
        // Context.stopService would result in the opposite outcome.
        Process.killProcess(mSecondBrowserServicePid);
        if (sync) {
            pollInstrumentationThread(new Callable<Boolean>() {
                @Override
                public Boolean call() throws Exception {
                    return !isSecondBrowserServiceRunning();
                }
            });
        }
        mSecondBrowserServicePid = 0;
    }

    private boolean tryStartingBrowserProcess() {
        final Boolean success[] = new Boolean[1];
        // The activity must be launched in order for proper webview statics to be setup.
        getActivity();
        // runOnMainSync does not catch RuntimeExceptions, they just terminate the test.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                try {
                    AwBrowserProcess.start();
                    success[0] = true;
                } catch (RuntimeException e) {
                    success[0] = false;
                }
            }
        });
        assertNotNull(success[0]);
        return success[0];
    }

    // Note that both onServiceDisconnected and Binder.DeathRecipient fire prematurely for our
    // purpose. We need to ensure that the service process has actually terminated, releasing all
    // the locks. The only reliable way to do that is to scan the process list.
    private boolean isSecondBrowserServiceRunning() {
        ActivityManager activityManager =
                (ActivityManager) getActivity().getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.RunningServiceInfo si : activityManager.getRunningServices(65536)) {
            if (si.pid == mSecondBrowserServicePid) return true;
        }
        return false;
    }
}
