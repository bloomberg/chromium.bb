// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.or;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Unit tests for ChildProcessConnection. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChildProcessConnectionTest {
    private static class ChildServiceConnectionMock
            implements ChildProcessConnection.ChildServiceConnection {
        private final Intent mBindIntent;
        private final ChildProcessConnection.ChildServiceConnectionDelegate mDelegate;
        private boolean mBound;

        public ChildServiceConnectionMock(
                Intent bindIntent, ChildProcessConnection.ChildServiceConnectionDelegate delegate) {
            mBindIntent = bindIntent;
            mDelegate = delegate;
        }

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

        public void notifyServiceConnected(IBinder service) {
            mDelegate.onServiceConnected(service);
        }

        public void notifyServiceDisconnected() {
            mDelegate.onServiceDisconnected();
        }

        public Intent getBindIntent() {
            return mBindIntent;
        }
    };

    private final ChildProcessConnection.ChildServiceConnectionFactory mServiceConnectionFactory =
            new ChildProcessConnection.ChildServiceConnectionFactory() {
                public ChildProcessConnection.ChildServiceConnection createConnection(
                        Intent bindIntent, int bindFlags,
                        ChildProcessConnection.ChildServiceConnectionDelegate delegate) {
                    ChildServiceConnectionMock connection =
                            spy(new ChildServiceConnectionMock(bindIntent, delegate));
                    if (mFirstServiceConnection == null) {
                        mFirstServiceConnection = connection;
                    }
                    return connection;
                }
            };

    @Mock
    private ChildProcessConnection.ServiceCallback mServiceCallback;

    @Mock
    private ChildProcessConnection.ConnectionCallback mConnectionCallback;

    private IChildProcessService mIChildProcessService;

    private Binder mChildProcessServiceBinder;

    private ChildServiceConnectionMock mFirstServiceConnection;

    // Parameters captured from the IChildProcessService.setupConnection() call
    private Bundle mConnectionBundle;
    private ICallbackInt mConnectionPidCallback;
    private IBinder mConnectionIBinderCallback;

    @Before
    public void setUp() throws RemoteException {
        MockitoAnnotations.initMocks(this);

        mIChildProcessService = mock(IChildProcessService.class);
        // Capture the parameters passed to the IChildProcessService.setupConnection() call.
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                mConnectionBundle = (Bundle) invocation.getArgument(0);
                mConnectionPidCallback = (ICallbackInt) invocation.getArgument(1);
                mConnectionIBinderCallback = (IBinder) invocation.getArgument(2);
                return null;
            }
        })
                .when(mIChildProcessService)
                .setupConnection(
                        or(isNull(), any(Bundle.class)), or(isNull(), any()), or(isNull(), any()));

        mChildProcessServiceBinder = new Binder();
        mChildProcessServiceBinder.attachInterface(
                mIChildProcessService, IChildProcessService.class.getName());
    }

    private ChildProcessConnection createDefaultTestConnection() {
        return createTestConnection(false /* bindToCaller */, false /* bindAsExternalService */,
                null /* serviceBundle */);
    }

    private ChildProcessConnection createTestConnection(
            boolean bindToCaller, boolean bindAsExternalService, Bundle serviceBundle) {
        String packageName = "org.chromium.test";
        String serviceName = "TestService";
        return new ChildProcessConnection(null /* context */,
                new ComponentName(packageName, serviceName), bindToCaller, bindAsExternalService,
                serviceBundle, mServiceConnectionFactory);
    }

    @Test
    public void testStrongBinding() {
        ChildProcessConnection connection = createDefaultTestConnection();
        connection.start(true /* useStrongBinding */, null /* serviceCallback */);
        assertTrue(connection.isStrongBindingBound());

        connection = createDefaultTestConnection();
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        assertFalse(connection.isStrongBindingBound());
    }

    @Test
    public void testServiceBundle() {
        Bundle serviceBundle = new Bundle();
        final String intKey = "org.chromium.myInt";
        final int intValue = 34;
        final int defaultValue = -1;
        serviceBundle.putInt(intKey, intValue);
        String stringKey = "org.chromium.myString";
        String stringValue = "thirty four";
        serviceBundle.putString(stringKey, stringValue);

        ChildProcessConnection connection = createTestConnection(
                false /* bindToCaller */, false /* bindAsExternalService */, serviceBundle);
        // Start the connection without the ChildServiceConnection connecting.
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        assertNotNull(mFirstServiceConnection);
        Intent bindIntent = mFirstServiceConnection.getBindIntent();
        assertNotNull(bindIntent);
        assertEquals(intValue, bindIntent.getIntExtra(intKey, defaultValue));
        assertEquals(stringValue, bindIntent.getStringExtra(stringKey));
    }

    @Test
    public void testServiceStartsSuccessfully() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
        Assert.assertTrue(connection.isInitialBindingBound());
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());

        // The service connects.
        mFirstServiceConnection.notifyServiceConnected(null /* iBinder */);
        Assert.assertTrue(connection.didOnServiceConnectedForTesting());
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(any());
    }

    @Test
    public void testServiceStartsAndFailsToBind() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        // Note we use doReturn so the actual bind() method is not called (it would with
        // when(mFirstServiceConnection.bind()).thenReturn(false).
        doReturn(false).when(mFirstServiceConnection).bind();
        connection.start(false /* useStrongBinding */, mServiceCallback);

        Assert.assertFalse(connection.isInitialBindingBound());
        Assert.assertFalse(connection.didOnServiceConnectedForTesting());
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testServiceStops() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
        mFirstServiceConnection.notifyServiceConnected(null /* iBinder */);
        connection.stop();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testServiceDisconnects() {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
        mFirstServiceConnection.notifyServiceConnected(null /* iBinder */);
        mFirstServiceConnection.notifyServiceDisconnected();
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, times(1)).onChildProcessDied(connection);
    }

    @Test
    public void testNotBoundToCaller() throws RemoteException {
        ChildProcessConnection connection = createTestConnection(false /* bindToCaller */,
                false /* bindAsExternalService */, null /* serviceBundle */);
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        // Service is started and bindToCallback is not called.
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(connection);
        verify(mIChildProcessService, never()).bindToCaller();
    }

    @Test
    public void testBoundToCallerSuccess() throws RemoteException {
        ChildProcessConnection connection = createTestConnection(true /* bindToCaller */,
                false /* bindAsExternalService */, null /* serviceBundle */);
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
        when(mIChildProcessService.bindToCaller()).thenReturn(true);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        // Service is started and bindToCallback is called.
        verify(mServiceCallback, times(1)).onChildStarted();
        verify(mServiceCallback, never()).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(connection);
        verify(mIChildProcessService, times(1)).bindToCaller();
    }

    @Test
    public void testBoundToCallerFailure() throws RemoteException {
        ChildProcessConnection connection = createTestConnection(true /* bindToCaller */,
                false /* bindAsExternalService */, null /* serviceBundle */);
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, mServiceCallback);
        // Pretend bindToCaller returns false, i.e. the service is already bound to a different
        // service.
        when(mIChildProcessService.bindToCaller()).thenReturn(false);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        // Service fails to start.
        verify(mServiceCallback, never()).onChildStarted();
        verify(mServiceCallback, times(1)).onChildStartFailed(any());
        verify(mServiceCallback, never()).onChildProcessDied(connection);
        verify(mIChildProcessService, times(1)).bindToCaller();
    }

    @Test
    public void testSetupConnectionBeforeServiceConnected() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        connection.setupConnection(
                null /* connectionBundle */, null /* callback */, mConnectionCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionPidCallback);
        mConnectionPidCallback.call(34 /* pid */);
        verify(mConnectionCallback, times(1)).onConnected(connection);
    }

    @Test
    public void testSetupConnectionAfterServiceConnected() throws RemoteException {
        ChildProcessConnection connection = createDefaultTestConnection();
        assertNotNull(mFirstServiceConnection);
        connection.start(false /* useStrongBinding */, null /* serviceCallback */);
        mFirstServiceConnection.notifyServiceConnected(mChildProcessServiceBinder);
        connection.setupConnection(
                null /* connectionBundle */, null /* callback */, mConnectionCallback);
        verify(mConnectionCallback, never()).onConnected(any());
        ShadowLooper.runUiThreadTasks();
        assertNotNull(mConnectionPidCallback);
        mConnectionPidCallback.call(34 /* pid */);
        verify(mConnectionCallback, times(1)).onConnected(connection);
    }
}
