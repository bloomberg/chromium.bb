// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell_apk;

import android.app.Service;
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
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.FileDescriptorInfo;
import org.chromium.content.browser.ChildProcessCreationParams;
import org.chromium.content.browser.ChildProcessLauncherHelper;

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
        final ChildProcessLauncherHelper processLauncher =
                ChildProcessLauncherTestUtils.startForTesting(true /* sandboxed */, commandLine,
                        new FileDescriptorInfo[0], params, true /* doSetupConnection */);

        // Poll the launcher until the connection is set up. The main test in
        // ChildProcessLauncherTest, which has bound the connection to this service, manages the
        // timeout via the lifetime of this service.
        final Handler handler = new Handler();
        final Runnable task = new Runnable() {
            final Messenger mReplyTo = msg.replyTo;

            @Override
            public void run() {
                int pid = 0;
                ChildProcessConnection conn = processLauncher.getChildProcessConnection();
                if (conn != null) {
                    pid = ChildProcessLauncherTestUtils.getConnectionPid(conn);
                }
                if (pid == 0) {
                    handler.postDelayed(this, 10 /* milliseconds */);
                    return;
                }

                try {
                    mReplyTo.send(Message.obtain(null, MSG_BIND_SERVICE_REPLY, pid,
                            ChildProcessLauncherTestUtils.getConnectionServiceNumber(conn)));
                } catch (RemoteException ex) {
                    throw new RuntimeException(ex);
                }
            }
        };
        handler.postDelayed(task, 10);
    }
}
