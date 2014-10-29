// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Handler;
import android.os.IBinder;

import org.chromium.components.devtools_bridge.ui.ServiceUIFactory;
import org.chromium.components.devtools_bridge.util.LooperExecutor;

import java.util.concurrent.Callable;

/**
 * Service to host DevToolsBridgeServer in tests.
 */
public class LocalBindingTestService extends Service {
    private DevToolsBridgeServer mServer;
    private final IBinder mBinder = new LocalBinder();
    private final String mSocketName;
    private boolean mForeground = false;

    private void checkCalledOnServiceThread() {
        assert getMainLooper().getThread() == Thread.currentThread();
    }

    public void invokeOnServiceThread(Runnable runnable) {
        try {
            invokeOnServiceThread(new TestUtils.RunnableAdapter(runnable));
        } catch (Exception e) {
            throw new RuntimeException("Unexpected exception", e);
        }
    }

    public <T> T invokeOnServiceThread(final Callable<T> callable) throws Exception {
        final TestUtils.InvokeHelper<T> helper = new TestUtils.InvokeHelper<T>();
        new Handler(getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                helper.runOnTargetThread(callable);
            }
        });
        return helper.takeResult();
    }

    protected LocalBindingTestService(String socketName) {
        mSocketName = socketName;
    }

    public LocalBindingTestService() {
        this(LocalBindingTestService.class.getName() + ".SERVER_SOCKET");
    }

    /**
     * Binder that provides direct access to the server for tests.
     */
    public class LocalBinder extends Binder {
        public SignalingReceiver getReceiver() {
            return mServer;
        }

        public SessionBase.Executor createExecutor() {
            return LooperExecutor.newInstanceForMainLooper(LocalBindingTestService.this);
        }

        public String socketName() {
            return mSocketName;
        }
    }

    @Override
    public void onCreate() {
        checkCalledOnServiceThread();

        mServer = new DevToolsBridgeServer(this, mSocketName, new TestServiceUIFactory()) {
            @Override
            protected void startForeground() {
                assert !mForeground;
                mForeground = true;
                // TODO: Investigate java.lang.NullPointerException at
                // android.app.Service.startForeground(Service.java:643).
                // super.startForeground() should be called.
            }

            @Override
            protected void stopForeground() {
                assert mForeground;
                mForeground = false;
            }
        };
    }

    @Override
    public void onDestroy() {
        if (mServer == null) return;

        checkCalledOnServiceThread();
        mServer.dispose();
        mServer = null;
    }

    @Override
    public IBinder onBind(Intent intent) {
        checkCalledOnServiceThread();

        return mBinder;
    }

    private static class TestServiceUIFactory extends ServiceUIFactory {
        protected String productName() {
            return "LocalBindingTestService";
        }

        protected int notificationSmallIcon() {
            return android.R.drawable.alert_dark_frame;
        }
    }
}
