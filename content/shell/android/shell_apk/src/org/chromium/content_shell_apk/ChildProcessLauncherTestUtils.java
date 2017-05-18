// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.content.Context;

import org.chromium.base.process_launcher.ChildProcessCreationParams;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.base.process_launcher.IChildProcessService;
import org.chromium.content.browser.ChildProcessConnection;
import org.chromium.content.browser.ChildProcessLauncherHelper;
import org.chromium.content.browser.LauncherThread;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.Semaphore;

/** An assortment of static methods used in tests that deal with launching child processes. */
public final class ChildProcessLauncherTestUtils {
    // Do not instanciate, use static methods instead.
    private ChildProcessLauncherTestUtils() {}

    public static void runOnLauncherThreadBlocking(final Runnable runnable) {
        if (LauncherThread.runningOnLauncherThread()) {
            runnable.run();
            return;
        }
        final Semaphore done = new Semaphore(0);
        LauncherThread.post(new Runnable() {
            @Override
            public void run() {
                runnable.run();
                done.release();
            }
        });
        done.acquireUninterruptibly();
    }

    public static <R> R runOnLauncherAndGetResult(Callable<R> callable) {
        if (LauncherThread.runningOnLauncherThread()) {
            try {
                return callable.call();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
        try {
            FutureTask<R> task = new FutureTask<R>(callable);
            LauncherThread.post(task);
            return task.get();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static ChildProcessLauncherHelper startForTesting(final Context context,
            final boolean sandboxed, final boolean alwaysInForeground, final String[] commandLine,
            final FileDescriptorInfo[] filesToBeMapped, final ChildProcessCreationParams params) {
        return runOnLauncherAndGetResult(new Callable<ChildProcessLauncherHelper>() {
            @Override
            public ChildProcessLauncherHelper call() {
                return ChildProcessLauncherHelper.createAndStartForTesting(0L /* nativePointer */,
                        commandLine, filesToBeMapped, params, sandboxed, alwaysInForeground);
            }
        });
    }

    public static ChildProcessConnection getConnection(
            final ChildProcessLauncherHelper childProcessLauncher) {
        return runOnLauncherAndGetResult(new Callable<ChildProcessConnection>() {
            @Override
            public ChildProcessConnection call() {
                return childProcessLauncher.getChildProcessConnection();
            }
        });
    }

    // Retrieves the PID of the passed in connection on the launcher thread as to not assert.
    public static int getConnectionPid(final ChildProcessConnection connection) {
        return runOnLauncherAndGetResult(new Callable<Integer>() {
            @Override
            public Integer call() {
                return connection.getPid();
            }
        });
    }

    // Retrieves the service number of the passed in connection from its service name, or -1 if the
    // service number could not be determined.
    public static int getConnectionServiceNumber(final ChildProcessConnection connection) {
        String serviceName = getConnectionServiceName(connection);
        // The service name ends up with the service number.
        StringBuilder numberString = new StringBuilder();
        for (int i = serviceName.length() - 1; i >= 0; i--) {
            char c = serviceName.charAt(i);
            if (!Character.isDigit(c)) {
                break;
            }
            numberString.append(c);
        }
        try {
            return Integer.decode(numberString.toString());
        } catch (NumberFormatException nfe) {
            return -1;
        }
    }

    // Retrieves the service number of the passed in connection on the launcher thread as to not
    // assert.
    public static String getConnectionServiceName(final ChildProcessConnection connection) {
        return runOnLauncherAndGetResult(new Callable<String>() {
            @Override
            public String call() {
                return connection.getServiceName().getClassName();
            }
        });
    }

    // Retrieves the service of the passed in connection on the launcher thread as to not assert.
    public static IChildProcessService getConnectionService(
            final ChildProcessConnection connection) {
        return runOnLauncherAndGetResult(new Callable<IChildProcessService>() {
            @Override
            public IChildProcessService call() {
                return connection.getService();
            }
        });
    }
}
