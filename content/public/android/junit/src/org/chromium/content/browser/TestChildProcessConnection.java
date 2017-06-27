// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.os.Bundle;

import org.chromium.base.process_launcher.ChildProcessCreationParams;

/** An implementation of ChildProcessConnection that does not connect to a real service. */
class TestChildProcessConnection extends ChildProcessConnection {
    private static class MockChildServiceConnection
            implements ChildProcessConnection.ChildServiceConnection {
        private boolean mBound;

        @Override
        public boolean bind() {
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

    /**
     * Creates a mock binding corresponding to real ManagedChildProcessConnection after the
     * connection is established: with initial binding bound and no strong binding.
     */
    TestChildProcessConnection(ComponentName serviceName, boolean bindAsExternalService,
            Bundle serviceBundle, ChildProcessCreationParams creationParams) {
        super(null /* context */, serviceName, bindAsExternalService, serviceBundle,
                creationParams);
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
        return new MockChildServiceConnection();
    }

    // We don't have a real service so we have to mock the connection status.
    @Override
    public void start(boolean useStrongBinding, ServiceCallback serviceCallback) {
        super.start(useStrongBinding, serviceCallback);
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
}
