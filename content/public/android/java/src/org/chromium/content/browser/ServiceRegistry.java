// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.mojo.bindings.Interface;
import org.chromium.mojo.bindings.Interface.Proxy;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.impl.CoreImpl;

/**
 * Java wrapper over Mojo ServiceRegistry held by the browser.
 */
@JNINamespace("content")
public class ServiceRegistry {

    interface ImplementationFactory<I extends Interface> {
        I createImpl();
    }

    <I extends Interface, P extends Proxy> void addService(
            Interface.Manager<I, P> manager, ImplementationFactory<I> factory) {
        nativeAddService(mNativeServiceRegistryAndroid, manager, factory, manager.getName());
    }

    <I extends Interface, P extends Proxy> void removeService(
            Interface.Manager<I, P> manager) {
        nativeRemoveService(mNativeServiceRegistryAndroid, manager.getName());
    }

    <I extends Interface, P extends Proxy> void connectToRemoteService(
            Interface.Manager<I, P> manager, InterfaceRequest<I> request) {
        int nativeHandle = request.passHandle().releaseNativeHandle();
        nativeConnectToRemoteService(
                mNativeServiceRegistryAndroid, manager.getName(), nativeHandle);
    }

    private long mNativeServiceRegistryAndroid;
    private final Core mCore;

    private ServiceRegistry(long nativeServiceRegistryAndroid, Core core) {
        mNativeServiceRegistryAndroid = nativeServiceRegistryAndroid;
        mCore = core;
    }

    @CalledByNative
    private static ServiceRegistry create(long nativeServiceRegistryAndroid) {
        return new ServiceRegistry(nativeServiceRegistryAndroid, CoreImpl.getInstance());
    }

    @CalledByNative
    private void destroy() {
        mNativeServiceRegistryAndroid = 0;
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

    private native void nativeAddService(long nativeServiceRegistryAndroid,
            Interface.Manager manager, ImplementationFactory factory, String name);
    private native void nativeRemoveService(long nativeServiceRegistryAndroid, String name);
    private native void nativeConnectToRemoteService(long nativeServiceRegistryAndroid, String name,
            int handle);
}
