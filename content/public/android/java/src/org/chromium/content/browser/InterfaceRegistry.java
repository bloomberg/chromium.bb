// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.mojo.bindings.Interface;
import org.chromium.mojo.bindings.Interface.Proxy;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.impl.CoreImpl;

/**
 * Java wrapper around the shell service's InterfaceRegistry type.
 */
@JNINamespace("content")
public class InterfaceRegistry {

    /**
     * The interface that a factory should implement.
     */
    public interface ImplementationFactory<I extends Interface> {
        I createImpl();
    }

    /**
     * Adds an interface factory.
     *
     * @param manager The interface manager.
     * @param factory The interface factory.
     */
    public <I extends Interface, P extends Proxy> void addInterface(
            Interface.Manager<I, P> manager, ImplementationFactory<I> factory) {
        nativeAddInterface(
                mNativeInterfaceRegistryAndroid, manager, factory, manager.getName());
    }

    <I extends Interface, P extends Proxy> void removeInterface(
            Interface.Manager<I, P> manager) {
        nativeRemoveInterface(mNativeInterfaceRegistryAndroid, manager.getName());
    }

    private long mNativeInterfaceRegistryAndroid;
    private final Core mCore;

    private InterfaceRegistry(long nativeInterfaceRegistryAndroid, Core core) {
        mNativeInterfaceRegistryAndroid = nativeInterfaceRegistryAndroid;
        mCore = core;
    }

    @CalledByNative
    private static InterfaceRegistry create(long nativeInterfaceRegistryAndroid) {
        return new InterfaceRegistry(nativeInterfaceRegistryAndroid,
                                     CoreImpl.getInstance());
    }

    @CalledByNative
    private void destroy() {
        mNativeInterfaceRegistryAndroid = 0;
    }

    // Declaring parametrized argument type for manager and factory breaks the JNI generator.
    // TODO(ppi): support parametrized argument types in the JNI generator.
    @SuppressWarnings("unchecked")
    @CalledByNative
    private void createImplAndAttach(int nativeHandle, Interface.Manager manager,
            ImplementationFactory factory) {
        MessagePipeHandle handle = mCore.acquireNativeHandle(nativeHandle).toMessagePipeHandle();
        manager.bind(factory.createImpl(), handle);
    }

    private native void nativeAddInterface(long nativeInterfaceRegistryAndroid,
            Interface.Manager manager, ImplementationFactory factory, String name);
    private native void nativeRemoveInterface(long nativeInterfaceRegistryAndroid, String name);
}
