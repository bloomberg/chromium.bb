// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content.browser.ServiceRegistry.ImplementationFactory;
import org.chromium.content_shell.ShellMojoTestUtils;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.mojo.bindings.ConnectionErrorHandler;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.bindings.test.mojom.math.Calculator;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;

import java.io.Closeable;
import java.util.ArrayList;
import java.util.List;

/**
 * Instrumentation tests for ServiceRegistry.
 */
public class ServiceRegistryTest extends ContentShellTestBase {

    private static final long RUN_LOOP_TIMEOUT_MS = 25;

    private final List<Closeable> mCloseablesToClose = new ArrayList<Closeable>();
    private final Core mCore = CoreImpl.getInstance();
    private long mNativeTestEnvironment;

    static class CalcConnectionErrorHandler implements ConnectionErrorHandler {

        MojoException mLastMojoException;

        @Override
        public void onConnectionError(MojoException e) {
            mLastMojoException = e;
        }
    }

    static class CalcCallback implements Calculator.AddResponse, Calculator.MultiplyResponse {

        double mResult = 0.0;

        @Override
        public void call(Double result) {
            mResult = result;
        }

        public double getResult() {
            return mResult;
        }
    }

    static class CalculatorImpl implements Calculator {

        double mResult = 0.0;

        @Override
        public void close() {}

        @Override
        public void onConnectionError(MojoException e) {}

        @Override
        public void clear(ClearResponse callback) {
            mResult = 0.0;
            callback.call(mResult);
        }

        @Override
        public void add(double value, AddResponse callback) {
            mResult += value;
            callback.call(mResult);
        }

        @Override
        public void multiply(double value, MultiplyResponse callback) {
            mResult *= value;
            callback.call(mResult);
        }
    }

    static class CalculatorFactory implements ImplementationFactory<Calculator> {

        @Override
        public Calculator createImpl() {
            return new CalculatorImpl();
        }
    }


    @Override
    protected void setUp() throws Exception {
        super.setUp();
        LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER).ensureInitialized();
        launchContentShellWithUrl("about://blank");
        mNativeTestEnvironment = ShellMojoTestUtils.setupTestEnvironment();
    }

    @Override
    protected void tearDown() throws Exception {
        for (Closeable c : mCloseablesToClose) {
            c.close();
        }
        ShellMojoTestUtils.tearDownTestEnvironment(mNativeTestEnvironment);
        mNativeTestEnvironment = 0;
        super.tearDown();
    }

    /**
     * Verifies that remote service can be requested and works.
     */
    @SmallTest
    public void testConnectToService() {
        Pair<ServiceRegistry, ServiceRegistry> registryPair =
                ShellMojoTestUtils.createServiceRegistryPair(mNativeTestEnvironment);
        ServiceRegistry serviceRegistryA = registryPair.first;
        ServiceRegistry serviceRegistryB = registryPair.second;

        // Add the Calculator service.
        serviceRegistryA.addService(Calculator.MANAGER, new CalculatorFactory());

        Pair<Calculator.Proxy, InterfaceRequest<Calculator>> requestPair =
                Calculator.MANAGER.getInterfaceRequest(mCore);

        mCloseablesToClose.add(requestPair.first);
        serviceRegistryB.connectToRemoteService(Calculator.MANAGER, requestPair.second);

        // Perform a few operations on the Calculator.
        Calculator.Proxy calculator = requestPair.first;
        CalcConnectionErrorHandler errorHandler = new CalcConnectionErrorHandler();
        calculator.setErrorHandler(errorHandler);
        CalcCallback callback = new CalcCallback();

        calculator.add(21, callback);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertEquals(21.0, callback.getResult());

        calculator.multiply(2, callback);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertEquals(42.0, callback.getResult());
    }

    /**
     * Verifies that a service can be requested only after it is added and not after it is removed.
     */
    @SmallTest
    public void testAddRemoveService() {
        Pair<ServiceRegistry, ServiceRegistry> registryPair =
                ShellMojoTestUtils.createServiceRegistryPair(mNativeTestEnvironment);
        ServiceRegistry serviceRegistryA = registryPair.first;
        ServiceRegistry serviceRegistryB = registryPair.second;

        // Request the Calculator service before it is added.
        Pair<Calculator.Proxy, InterfaceRequest<Calculator>> requestPair =
                Calculator.MANAGER.getInterfaceRequest(mCore);
        Calculator.Proxy calculator = requestPair.first;
        CalcConnectionErrorHandler errorHandler = new CalcConnectionErrorHandler();
        calculator.setErrorHandler(errorHandler);
        mCloseablesToClose.add(calculator);
        serviceRegistryB.connectToRemoteService(Calculator.MANAGER, requestPair.second);

        // Spin the message loop and verify that an error occured.
        assertNull(errorHandler.mLastMojoException);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertNotNull(errorHandler.mLastMojoException);

        // Add the Calculator service and request it again.
        errorHandler.mLastMojoException = null;
        serviceRegistryA.addService(Calculator.MANAGER, new CalculatorFactory());
        requestPair = Calculator.MANAGER.getInterfaceRequest(mCore);
        calculator = requestPair.first;
        errorHandler = new CalcConnectionErrorHandler();
        mCloseablesToClose.add(calculator);
        serviceRegistryB.connectToRemoteService(Calculator.MANAGER, requestPair.second);

        // Spin the message loop and verify that no error occured.
        assertNull(errorHandler.mLastMojoException);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertNull(errorHandler.mLastMojoException);

        // Remove the Calculator service and request it again.
        errorHandler.mLastMojoException = null;
        serviceRegistryA.removeService(Calculator.MANAGER);
        requestPair = Calculator.MANAGER.getInterfaceRequest(mCore);
        calculator = requestPair.first;
        errorHandler = new CalcConnectionErrorHandler();
        calculator.setErrorHandler(errorHandler);
        mCloseablesToClose.add(calculator);
        serviceRegistryB.connectToRemoteService(Calculator.MANAGER, requestPair.second);

        // Spin the message loop and verify that an error occured.
        assertNull(errorHandler.mLastMojoException);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertNotNull(errorHandler.mLastMojoException);
    }
}
