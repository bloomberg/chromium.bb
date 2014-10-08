// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.content.browser.ServiceRegistry.ImplementationFactory;
import org.chromium.content_shell.ShellMojoTestUtils;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.bindings.test.mojom.math.Calculator;
import org.chromium.mojo.bindings.test.mojom.math.CalculatorUi;
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

    static class CalculatorImpl implements Calculator {

        double mResult = 0.0;
        CalculatorUi mClient;

        @Override
        public void close() {}

        @Override
        public void onConnectionError(MojoException e) {}

        @Override
        public void clear() {
            mResult = 0.0;
            mClient.output(mResult);
        }

        @Override
        public void add(double value) {
            mResult += value;
            mClient.output(mResult);
        }

        @Override
        public void multiply(double value) {
            mResult *= value;
            mClient.output(mResult);
        }

        @Override
        public void setClient(CalculatorUi client) {
            mClient = client;
        }
    }

    static class CalculatorFactory implements ImplementationFactory<Calculator> {

        @Override
        public Calculator createImpl() {
            return new CalculatorImpl();
        }
    }

    static class CalculatorUiImpl implements CalculatorUi {

        double mOutput = 0.0;
        MojoException mLastMojoException;

        @Override
        public void close() {}

        @Override
        public void onConnectionError(MojoException e) {
            mLastMojoException = e;
        }

        @Override
        public void output(double value) {
            mOutput = value;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        LibraryLoader.ensureInitialized();
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

        // Create an instance of CalculatorUi and request a Calculator service for it.
        CalculatorUiImpl calculatorUi = new CalculatorUiImpl();
        Pair<Calculator.Proxy, InterfaceRequest<Calculator>> requestPair =
                Calculator.MANAGER.getInterfaceRequest(mCore, calculatorUi);
        mCloseablesToClose.add(requestPair.first);
        serviceRegistryB.connectToRemoteService(Calculator.MANAGER, requestPair.second);

        // Perform a few operations on the Calculator.
        Calculator.Proxy calculator = requestPair.first;
        calculator.add(21);
        calculator.multiply(2);

        // Spin the message loop and verify the results.
        assertEquals(0.0, calculatorUi.mOutput);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertEquals(42.0, calculatorUi.mOutput);
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
        CalculatorUiImpl calculatorUi = new CalculatorUiImpl();
        Pair<Calculator.Proxy, InterfaceRequest<Calculator>> requestPair =
                Calculator.MANAGER.getInterfaceRequest(mCore, calculatorUi);
        mCloseablesToClose.add(requestPair.first);
        serviceRegistryB.connectToRemoteService(Calculator.MANAGER, requestPair.second);

        // Spin the message loop and verify that an error occured.
        assertNull(calculatorUi.mLastMojoException);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertNotNull(calculatorUi.mLastMojoException);

        // Add the Calculator service and request it again.
        calculatorUi.mLastMojoException = null;
        serviceRegistryA.addService(Calculator.MANAGER, new CalculatorFactory());
        requestPair = Calculator.MANAGER.getInterfaceRequest(mCore, calculatorUi);
        mCloseablesToClose.add(requestPair.first);
        serviceRegistryB.connectToRemoteService(Calculator.MANAGER, requestPair.second);

        // Spin the message loop and verify that no error occured.
        assertNull(calculatorUi.mLastMojoException);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertNull(calculatorUi.mLastMojoException);

        // Remove the Calculator service and request it again.
        calculatorUi.mLastMojoException = null;
        serviceRegistryA.removeService(Calculator.MANAGER);
        requestPair = Calculator.MANAGER.getInterfaceRequest(mCore, calculatorUi);
        mCloseablesToClose.add(requestPair.first);
        serviceRegistryB.connectToRemoteService(Calculator.MANAGER, requestPair.second);

        // Spin the message loop and verify that an error occured.
        assertNull(calculatorUi.mLastMojoException);
        ShellMojoTestUtils.runLoop(RUN_LOOP_TIMEOUT_MS);
        assertNotNull(calculatorUi.mLastMojoException);
    }
}
