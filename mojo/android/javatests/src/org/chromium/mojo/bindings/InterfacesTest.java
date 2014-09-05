// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.mojo.MojoTestCase;
import org.chromium.mojo.bindings.BindingsTestUtils.CapturingErrorHandler;
import org.chromium.mojo.bindings.test.mojom.imported.ImportedInterface;
import org.chromium.mojo.bindings.test.mojom.sample.Factory;
import org.chromium.mojo.bindings.test.mojom.sample.FactoryClient;
import org.chromium.mojo.bindings.test.mojom.sample.NamedObject;
import org.chromium.mojo.bindings.test.mojom.sample.NamedObject.GetNameResponse;
import org.chromium.mojo.bindings.test.mojom.sample.Request;
import org.chromium.mojo.bindings.test.mojom.sample.Response;
import org.chromium.mojo.system.DataPipe.ConsumerHandle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;

import java.io.Closeable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests for interfaces / proxies / stubs generated for sample_factory.mojom.
 */
public class InterfacesTest extends MojoTestCase {

    private static final long RUN_LOOP_TIMEOUT_MS = 25;

    private final List<Closeable> mCloseablesToClose = new ArrayList<Closeable>();

    /**
     * Basic implementation of {@link NamedObject}.
     */
    public static class MockNamedObjectImpl extends CapturingErrorHandler implements NamedObject {

        private String mName = "";

        /**
         * @see org.chromium.mojo.bindings.Interface#close()
         */
        @Override
        public void close() {
        }

        @Override
        public void setName(String name) {
            mName = name;
        }

        @Override
        public void getName(GetNameResponse callback) {
            callback.call(mName);
        }

        public String getNameSynchronously() {
            return mName;
        }
    }

    /**
     * Implementation of {@link GetNameResponse} keeping track of usage.
     */
    public static class RecordingGetNameResponse implements GetNameResponse {
        private String mName;
        private boolean mCalled;

        public RecordingGetNameResponse() {
            reset();
        }

        @Override
        public void call(String name) {
            mName = name;
            mCalled = true;
        }

        public String getName() {
            return mName;
        }

        public boolean wasCalled() {
            return mCalled;
        }

        public void reset() {
            mName = null;
            mCalled = false;
        }
    }

    /**
     * Basic implementation of {@link Factory}.
     */
    public class MockFactoryImpl extends CapturingErrorHandler implements Factory {

        private boolean mClosed = false;
        private FactoryClient mFactoryClient;

        public boolean isClosed() {
            return mClosed;
        }

        @Override
        public void setClient(FactoryClient client) {
            mFactoryClient = client;
            mCloseablesToClose.add(client);
        }

        /**
         * @see org.chromium.mojo.bindings.Interface#close()
         */
        @Override
        public void close() {
            mClosed = true;
        }

        @Override
        public void doStuff(Request request, MessagePipeHandle pipe) {
            if (pipe != null) {
                pipe.close();
            }
            Response response = new Response();
            response.x = 42;
            mFactoryClient.didStuff(response, "Hello");
        }

        @Override
        public void doStuff2(ConsumerHandle pipe) {
        }

        @Override
        public void createNamedObject(InterfaceRequest<NamedObject> obj) {
            NamedObject.MANAGER.bind(new MockNamedObjectImpl(), obj);
        }

        @Override
        public void requestImportedInterface(InterfaceRequest<ImportedInterface> obj,
                RequestImportedInterfaceResponse callback) {
            throw new UnsupportedOperationException("Not implemented.");
        }

        @Override
        public void takeImportedInterface(ImportedInterface obj,
                TakeImportedInterfaceResponse callback) {
            throw new UnsupportedOperationException("Not implemented.");
        }
    }

    /**
     * Basic implementation of {@link FactoryClient}.
     */
    public static class MockFactoryClientImpl implements FactoryClient {

        private boolean mClosed = false;
        private boolean mDidStuffCalled = false;

        public boolean isClosed() {
            return mClosed;
        }

        public boolean wasDidStuffCalled() {
            return mDidStuffCalled;
        }

        /**
         * @see org.chromium.mojo.bindings.Interface#close()
         */
        @Override
        public void close() {
            mClosed = true;
        }

        /**
         * @see ConnectionErrorHandler#onConnectionError(MojoException)
         */
        @Override
        public void onConnectionError(MojoException e) {
        }

        /**
         * @see FactoryClient#didStuff(Response, java.lang.String)
         */
        @Override
        public void didStuff(Response response, String text) {
            mDidStuffCalled = true;
        }

        /**
         * @see FactoryClient#didStuff2(String)
         */
        @Override
        public void didStuff2(String text) {
        }

    }

    /**
     * @see MojoTestCase#tearDown()
     */
    @Override
    protected void tearDown() throws Exception {
        // Close the elements in the reverse order they were added. This is needed because it is an
        // error to close the handle of a proxy without closing the proxy first.
        Collections.reverse(mCloseablesToClose);
        for (Closeable c : mCloseablesToClose) {
            c.close();
        }
        super.tearDown();
    }

    private <I extends Interface, P extends Interface.Proxy> P newProxyOverPipe(
            Interface.Manager<I, P> manager, I impl) {
        Pair<MessagePipeHandle, MessagePipeHandle> handles =
                CoreImpl.getInstance().createMessagePipe(null);
        P proxy = manager.attachProxy(handles.first);
        mCloseablesToClose.add(proxy);
        manager.bind(impl, handles.second);
        return proxy;
    }

    private <I extends InterfaceWithClient<C>, P extends InterfaceWithClient.Proxy<C>,
            C extends Interface> P newProxyOverPipeWithClient(
            InterfaceWithClient.Manager<I, P, C> manager, I impl, C client) {
        Pair<MessagePipeHandle, MessagePipeHandle> handles =
                CoreImpl.getInstance().createMessagePipe(null);
        P proxy = manager.attachProxy(handles.first, client);
        mCloseablesToClose.add(proxy);
        manager.bind(impl, handles.second);
        return proxy;
    }

    /**
     * Check that the given proxy receives the calls. If |impl| is not null, also check that the
     * calls are forwared to |impl|.
     */
    private void checkProxy(NamedObject.Proxy proxy, MockNamedObjectImpl impl) {
        final String NAME = "hello world";
        RecordingGetNameResponse callback = new RecordingGetNameResponse();
        CapturingErrorHandler errorHandler = new CapturingErrorHandler();
        proxy.setErrorHandler(errorHandler);

        if (impl != null) {
            assertNull(impl.getLastMojoException());
            assertEquals("", impl.getNameSynchronously());
        }

        proxy.getName(callback);
        nativeRunLoop(RUN_LOOP_TIMEOUT_MS);

        assertNull(errorHandler.getLastMojoException());
        assertTrue(callback.wasCalled());
        assertEquals("", callback.getName());

        callback.reset();
        proxy.setName(NAME);
        nativeRunLoop(RUN_LOOP_TIMEOUT_MS);

        assertNull(errorHandler.getLastMojoException());
        if (impl != null) {
            assertNull(impl.getLastMojoException());
            assertEquals(NAME, impl.getNameSynchronously());
        }

        proxy.getName(callback);
        nativeRunLoop(RUN_LOOP_TIMEOUT_MS);

        assertNull(errorHandler.getLastMojoException());
        assertTrue(callback.wasCalled());
        assertEquals(NAME, callback.getName());
    }

    @SmallTest
    public void testName() {
        assertEquals("sample::NamedObject", NamedObject.MANAGER.getName());
    }

    @SmallTest
    public void testProxyAndStub() {
        MockNamedObjectImpl impl = new MockNamedObjectImpl();
        NamedObject.Proxy proxy =
                NamedObject.MANAGER.buildProxy(null, NamedObject.MANAGER.buildStub(null, impl));

        checkProxy(proxy, impl);
    }

    @SmallTest
    public void testProxyAndStubOverPipe() {
        MockNamedObjectImpl impl = new MockNamedObjectImpl();
        NamedObject.Proxy proxy = newProxyOverPipe(NamedObject.MANAGER, impl);

        checkProxy(proxy, impl);
    }

    @SmallTest
    public void testFactoryOverPipe() {
        Factory.Proxy proxy = newProxyOverPipe(Factory.MANAGER, new MockFactoryImpl());
        Pair<NamedObject.Proxy, InterfaceRequest<NamedObject>> request =
                NamedObject.MANAGER.getInterfaceRequest(CoreImpl.getInstance());
        mCloseablesToClose.add(request.first);
        proxy.createNamedObject(request.second);

        checkProxy(request.first, null);
    }

    @SmallTest
    public void testInterfaceClosing() {
        MockFactoryImpl impl = new MockFactoryImpl();
        MockFactoryClientImpl client = new MockFactoryClientImpl();
        Factory.Proxy proxy = newProxyOverPipeWithClient(
                Factory.MANAGER, impl, client);

        assertFalse(impl.isClosed());
        assertFalse(client.isClosed());

        proxy.close();
        nativeRunLoop(RUN_LOOP_TIMEOUT_MS);

        assertTrue(impl.isClosed());
        assertTrue(client.isClosed());
    }

    @SmallTest
    public void testClient() {
        MockFactoryImpl impl = new MockFactoryImpl();
        MockFactoryClientImpl client = new MockFactoryClientImpl();
        Factory.Proxy proxy = newProxyOverPipeWithClient(
                Factory.MANAGER, impl, client);
        Request request = new Request();
        request.x = 42;
        proxy.doStuff(request, null);

        assertFalse(client.wasDidStuffCalled());

        nativeRunLoop(RUN_LOOP_TIMEOUT_MS);

        assertTrue(client.wasDidStuffCalled());
    }
}
