// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.ServiceRegistry;
import org.chromium.mojo.system.Pair;

/**
 * Test hooks for Mojo service support in the browser. See http://crbug.com/415945.
 */
@JNINamespace("content")
public class ShellMojoTestUtils {
    public static long setupTestEnvironment() {
        return nativeSetupTestEnvironment();
    }

    public static void tearDownTestEnvironment(long testEnvironment) {
        nativeTearDownTestEnvironment(testEnvironment);
    }

    /**
     * Yields two ServiceRegistries bound to each other.
     */
    public static Pair<ServiceRegistry, ServiceRegistry> createServiceRegistryPair(
            long testEnvironment) {
        // Declaring parametrized return type for nativeCreateServiceRegistryPair() breaks the JNI
        // generator. TODO(ppi): support parametrized return types in the JNI generator.
        @SuppressWarnings("unchecked")
        Pair<ServiceRegistry, ServiceRegistry> pair =
                nativeCreateServiceRegistryPair(testEnvironment);
        return pair;
    }

    public static void runLoop(long timeoutMs) {
        nativeRunLoop(timeoutMs);
    }

    @CalledByNative
    public static Pair makePair(ServiceRegistry serviceRegistryA,
            ServiceRegistry serviceRegistryB) {
        return new Pair<ServiceRegistry, ServiceRegistry>(serviceRegistryA, serviceRegistryB);
    }

    private static native long nativeSetupTestEnvironment();
    private static native void nativeTearDownTestEnvironment(long testEnvironment);
    private static native Pair nativeCreateServiceRegistryPair(long testEnvironment);
    private static native void nativeRunLoop(long timeoutMs);
}
