// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.process_launcher.ChildProcessCreationParams;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.content.browser.ChildProcessConnection;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.browser.LauncherThread;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.Semaphore;

/**
 * A Service that assists the ChildProcessLauncherTest that responds to one message, which
 * starts a sandboxed service process via the ChildProcessLauncher. This is required to test
 * the behavior when two independent processes in the same package try and bind to the same
 * sandboxed service process.
 */
public class ChildProcessLauncherTestHelperService extends Service {
    public static final int MSG_BIND_SERVICE = IBinder.FIRST_CALL_TRANSACTION + 1;
    public static final int MSG_BIND_SERVICE_REPLY = MSG_BIND_SERVICE + 1;

    private final Handler.Callback mHandlerCallback = new Handler.Callback() {
        @Override
        public boolean handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_BIND_SERVICE:
                    doBindService(msg);
                    return true;
            }
            return false;
        }
    };

    private final HandlerThread mHandlerThread = new HandlerThread("Helper Service Handler");

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

    public static ChildProcessConnection startInternalForTesting(final Context context,
            final String[] commandLine, final FileDescriptorInfo[] filesToMap,
            final ChildProcessCreationParams params) {
        return runOnLauncherAndGetResult(new Callable<ChildProcessConnection>() {
            @Override
            public ChildProcessConnection call() {
                return ChildProcessLauncher.startInternal(context, commandLine,
                        0 /* childProcessId */, filesToMap, null /* launchCallback */,
                        null /* childProcessCallback */, true /* inSandbox */,
                        false /* alwaysInForeground */, params);
            }
        });
    }

    @Override
    public void onCreate() {
        CommandLine.init(null);
        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_CHILD).ensureInitialized();
        } catch (ProcessInitException ex) {
            throw new RuntimeException(ex);
        }

        mHandlerThread.start();
    }

    @Override
    public IBinder onBind(Intent intent) {
        Messenger messenger =
                new Messenger(new Handler(mHandlerThread.getLooper(), mHandlerCallback));
        return messenger.getBinder();
    }

    private void doBindService(final Message msg) {
        String[] commandLine = { "_", "--" + BaseSwitches.RENDERER_WAIT_FOR_JAVA_DEBUGGER };
        final boolean bindToCaller = true;
        ChildProcessCreationParams params = new ChildProcessCreationParams(
                getPackageName(), false, LibraryProcessType.PROCESS_CHILD, bindToCaller);
        final ChildProcessConnection conn =
                startInternalForTesting(this, commandLine, new FileDescriptorInfo[0], params);

        // Poll the connection until it is set up. The main test in ChildProcessLauncherTest, which
        // has bound the connection to this service, manages the timeout via the lifetime of this
        // service.
        final Handler handler = new Handler();
        final Runnable task = new Runnable() {
            final Messenger mReplyTo = msg.replyTo;

            @Override
            public void run() {
                if (conn.getPid() != 0) {
                    try {
                        mReplyTo.send(Message.obtain(null, MSG_BIND_SERVICE_REPLY, conn.getPid(),
                                    conn.getServiceNumber()));
                    } catch (RemoteException ex) {
                        throw new RuntimeException(ex);
                    }
                } else {
                    handler.postDelayed(this, 10 /* milliseconds */);
                }
            }
        };
        handler.postDelayed(task, 10);
    }
}
