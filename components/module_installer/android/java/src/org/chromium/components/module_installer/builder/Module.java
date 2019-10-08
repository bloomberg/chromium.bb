// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import org.chromium.base.StrictModeContext;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.module_installer.engine.InstallEngine;
import org.chromium.components.module_installer.engine.InstallListener;
import org.chromium.components.module_installer.util.Timer;

import java.util.HashSet;
import java.util.Set;
import java.util.TreeSet;

/**
 * Represents a feature module. Can be used to install the module, access its interface, etc. See
 * {@link ModuleInterface} for how to conveniently create an instance of the module class for a
 * specific feature module.
 *
 * @param <T> The interface of the module
 */
@JNINamespace("module_installer")
public class Module<T> {
    private static final Set<String> sInitializedModules = new HashSet<>();
    private static Set<String> sPendingNativeRegistrations = new TreeSet<>();

    private final String mName;
    private final Class<T> mInterfaceClass;
    private final String mImplClassName;
    private T mImpl;
    private InstallEngine mInstaller;

    /**
     * To be called after the main native library has been loaded. Any module instances created
     * before the native library is loaded have their native component queued for loading and
     * registration. Calling this methed completes that process.
     */
    public static void doDeferredNativeRegistrations() {
        if (sPendingNativeRegistrations == null) return;
        for (String name : sPendingNativeRegistrations) {
            loadNative(name);
        }
        sPendingNativeRegistrations = null;
    }

    /**
     * Instantiates a module.
     *
     * @param name The module's name as used with {@link ModuleInstaller}.
     * @param interfaceClass {@link Class} object of the module interface.
     * @param implClassName fully qualified class name of the implementation of the module's
     *                      interface.
     */
    public Module(String name, Class<T> interfaceClass, String implClassName) {
        mName = name;
        mInterfaceClass = interfaceClass;
        mImplClassName = implClassName;
    }

    @VisibleForTesting
    public InstallEngine getInstallEngine() {
        if (mInstaller == null) {
            try (Timer timer = new Timer()) {
                mInstaller = new ModuleEngine(mImplClassName);
            }
        }
        return mInstaller;
    }

    @VisibleForTesting
    public void setInstallEngine(InstallEngine engine) {
        mInstaller = engine;
    }

    /**
     * Returns true if the module is currently installed and can be accessed.
     */
    public boolean isInstalled() {
        try (Timer timer = new Timer()) {
            return getInstallEngine().isInstalled(mName);
        }
    }

    /**
     * Requests install of the module.
     */
    public void install(InstallListener listener) {
        try (Timer timer = new Timer()) {
            assert !isInstalled();
            getInstallEngine().install(mName, listener);
        }
    }

    /**
     * Requests deferred install of the module.
     */
    public void installDeferred() {
        try (Timer timer = new Timer()) {
            getInstallEngine().installDeferred(mName);
        }
    }

    /**
     * Returns the implementation of the module interface. Must only be called if the module is
     * installed.
     */
    public T getImpl() {
        try (Timer timer = new Timer()) {
            if (mImpl != null) return mImpl;

            assert isInstalled();
            // Accessing classes in the module may cause its DEX file to be loaded. And on some
            // devices that causes a read mode violation.
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                mImpl = mInterfaceClass.cast(Class.forName(mImplClassName).newInstance());
            } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                    | IllegalArgumentException e) {
                throw new RuntimeException(e);
            }

            // Load the module's native code and/or resources if they are present, and the Chrome
            // native library itself has been loaded.
            if (sPendingNativeRegistrations == null) {
                loadNative(mName);
            } else {
                // We have to defer native initialization because VR is calling getImpl in early
                // startup. As soon as VR stops doing that we want to deprecate deferred native
                // initialization.
                sPendingNativeRegistrations.add(mName);
            }
            return mImpl;
        }
    }

    private static void loadNative(String name) {
        // Can only initialize native once per lifetime of Chrome.
        if (sInitializedModules.contains(name)) return;
        // TODO(crbug.com/870055, crbug.com/986960): Automatically determine if module has native
        // code or resources instead of whitelisting.
        boolean loadLibrary = false;
        boolean loadResources = false;
        if ("test_dummy".equals(name)) {
            loadLibrary = true;
            loadResources = true;
        }
        if ("dev_ui".equals(name)) {
            loadResources = true;
        }
        if (loadLibrary || loadResources) {
            ModuleJni.get().loadNative(name, loadLibrary, loadResources);
        }
        sInitializedModules.add(name);
    }

    @NativeMethods
    interface Natives {
        void loadNative(String name, boolean loadLibrary, boolean loadResources);
    }
}
