// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import org.chromium.base.StrictModeContext;

/**
 * Represents a feature module. Can be used to install the module, access its interface, etc. See
 * {@link ModuleInterface} for how to conveniently create an instance of the module class for a
 * specific feature module.
 *
 * @param <T> The interface of the module/
 */
public class Module<T> {
    private final String mName;
    private final Class<T> mInterfaceClass;
    private final String mImplClassName;
    private T mImpl;

    /**
     * Instantiates a module.
     *
     * @param name The module's name as used with {@link ModuleInstaller}.
     * @param interfaceClass {@link Class} object of the module interface.
     * @param implClassName fully qualified class name of the implementation of the module's
     *interface.
     **/
    public Module(String name, Class<T> interfaceClass, String implClassName) {
        mName = name;
        mInterfaceClass = interfaceClass;
        mImplClassName = implClassName;
    }

    /** Returns true if the module is currently installed and can be accessed. */
    public boolean isInstalled() {
        if (mImpl != null) return true;
        // Accessing classes in the module may cause its DEX file to be loaded. And on some devices
        // that causes a read mode violation.
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            ModuleInstaller.init();
            Class.forName(mImplClassName);
            return true;
        } catch (ClassNotFoundException e) {
            return false;
        }
    }

    /** Requests install of the module. See {@link ModuleInstaller#install} for more details. */
    public void install(OnModuleInstallFinishedListener onFinishedListener) {
        assert !isInstalled();
        ModuleInstaller.install(mName, onFinishedListener);
    }

    /**
     * Requests deferred install of the module. See {@link ModuleInstaller#installDeferred} for
     * more details.
     */
    public void installDeferred() {
        ModuleInstaller.installDeferred(mName);
    }

    /**
     * Returns the implementation of the module interface. Must only be called if the module is
     * installed.
     */
    public T getImpl() {
        assert isInstalled();
        if (mImpl == null) {
            ModuleInstaller.init();
            // Accessing classes in the module may cause its DEX file to be loaded. And on some
            // devices that causes a read mode violation.
            try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
                mImpl = mInterfaceClass.cast(Class.forName(mImplClassName).newInstance());
            } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                    | IllegalArgumentException e) {
                throw new RuntimeException(e);
            }
        }
        return mImpl;
    }
}
