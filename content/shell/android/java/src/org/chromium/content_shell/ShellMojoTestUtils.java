// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.InterfaceProvider;
import org.chromium.content.browser.InterfaceRegistry;
import org.chromium.mojo.system.Pair;

/**
 * Test hooks for shell InterfaceRegistry/Provider support in the browser.
 * See http://crbug.com/415945.
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
     * Returns an InterfaceRegistry and an InterfaceProvider bound to it.
     */
    public static Pair<InterfaceRegistry, InterfaceProvider> createInterfaceRegistryAndProvider(
            long testEnvironment) {
        // Declaring parametrized return type for nativeCreateInterfaceRegistryAndProvider() breaks
        // the JNI generator. TODO(ppi): support parametrized return types in the JNI generator.
        @SuppressWarnings("unchecked")
        Pair<InterfaceRegistry, InterfaceProvider> pair =
                nativeCreateInterfaceRegistryAndProvider(testEnvironment);
        return pair;
    }

    public static void runLoop(long timeoutMs) {
        nativeRunLoop(timeoutMs);
    }

    @CalledByNative
    public static Pair makePair(InterfaceRegistry registry, InterfaceProvider provider) {
        return new Pair<InterfaceRegistry, InterfaceProvider>(registry, provider);
    }

    private static native long nativeSetupTestEnvironment();
    private static native void nativeTearDownTestEnvironment(long testEnvironment);
    private static native Pair nativeCreateInterfaceRegistryAndProvider(long testEnvironment);
    private static native void nativeRunLoop(long timeoutMs);
}
