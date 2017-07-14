// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.os.Bundle;
import android.os.IBinder;

import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.process_launcher.ICallbackInt;
import org.chromium.base.process_launcher.IChildProcessService;

/** An implementation of ChildProcessConnection that does not connect to a real service. */
class TestChildProcessConnection extends ChildProcessConnection {
    private static class MockServiceBinder extends IChildProcessService.Stub {
        @Override
        public boolean bindToCaller() {
            return true;
        }

        @Override
        public void setupConnection(Bundle args, ICallbackInt pidCallback, IBinder gpuCallback) {}

        @Override
        public void crashIntentionallyForTesting() {
            throw new RuntimeException("crashIntentionallyForTesting");
        }
    }

    private class MockChildServiceConnection
            implements ChildProcessConnection.ChildServiceConnection {
        private final ChildProcessConnection mConnection;
        private boolean mBound;

        MockChildServiceConnection(ChildProcessConnection connection) {
            mConnection = connection;
        }

        @Override
        public boolean bind() {
            if (TestChildProcessConnection.this.mPostOnServiceConnected) {
                LauncherThread.post(new Runnable() {
                    @Override
                    public void run() {
                        // TODO(boliu): implement a dummy service.
                        mConnection.onServiceConnectedOnLauncherThread(new MockServiceBinder());
                    }
                });
            }
            mBound = true;
            return true;
        }

        @Override
        public void unbind() {
            mBound = false;
        }

        @Override
        public boolean isBound() {
            return mBound;
        }
    }

    private int mPid;
    private boolean mConnected;
    private ServiceCallback mServiceCallback;
    private boolean mPostOnServiceConnected;

    /**
     * Creates a mock binding corresponding to real ManagedChildProcessConnection after the
     * connection is established: with initial binding bound and no strong binding.
     */
    TestChildProcessConnection(ComponentName serviceName, boolean bindToCaller,
            boolean bindAsExternalService, Bundle serviceBundle) {
        super(null /* context */, serviceName, bindToCaller, bindAsExternalService, serviceBundle);
        mPostOnServiceConnected = true;
    }

    public void setPid(int pid) {
        mPid = pid;
    }

    @Override
    public int getPid() {
        return mPid;
    }

    @Override
    protected ChildServiceConnection createServiceConnection(int bindFlags) {
        return new MockChildServiceConnection(this);
    }

    // We don't have a real service so we have to mock the connection status.
    @Override
    public void start(
            boolean useStrongBinding, ServiceCallback serviceCallback, boolean retryOnTimeout) {
        super.start(useStrongBinding, serviceCallback, retryOnTimeout);
        mConnected = true;
        mServiceCallback = serviceCallback;
    }

    @Override
    public void stop() {
        super.stop();
        mConnected = false;
    }

    @Override
    public boolean isConnected() {
        return mConnected;
    }

    public ServiceCallback getServiceCallback() {
        return mServiceCallback;
    }

    public void setPostOnServiceConnected(boolean post) {
        mPostOnServiceConnected = post;
    }
}
