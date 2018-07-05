// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * Dynamically loads a module from another apk.
 */
public final class ModuleLoader {
    private static final String TAG = "ModuleLoader";

    private ModuleLoader() {}

    /**
     * Dynamically loads the class {@code className} from the application identified by
     * {@code packageName} and wraps it in a {@link ModuleEntryPoint}.
     * @param packageName The package name of the application to load form.
     * @param className The name of the class to load, in the form of a binary name (including the
     *     class package name).
     * @return The loaded class, cast to an AIDL interface and wrapped in a more user friendly form.
     */
    @Nullable
    public static ModuleEntryPoint loadModule(String packageName, String className) {
        Context applicationContext = ContextUtils.getApplicationContext();
        Context moduleContext = getModuleContext(applicationContext, packageName);

        if (moduleContext == null) return null;

        try {
            long entryPointLoadClassStartTime = ModuleMetrics.now();
            Class<?> clazz = moduleContext.getClassLoader().loadClass(className);
            ModuleMetrics.recordLoadClassTime(entryPointLoadClassStartTime);

            long entryPointNewInstanceStartTime = ModuleMetrics.now();
            IBinder binder = (IBinder) clazz.newInstance();
            ModuleMetrics.recordEntryPointNewInstanceTime(entryPointNewInstanceStartTime);

            ModuleHostImpl moduleHost = new ModuleHostImpl(applicationContext, moduleContext);
            ModuleEntryPoint entryPoint =
                    new ModuleEntryPoint(IModuleEntryPoint.Stub.asInterface(binder));

            if (!isCompatible(moduleHost, entryPoint)) {
                Log.w(TAG, "Incompatible module due to version mismatch: host version %s, "
                                + "minimum required host version %s, entry point version %s, "
                                + "minimum required entry point version %s.",
                        moduleHost.getHostVersion(), entryPoint.getMinimumHostVersion(),
                        entryPoint.getModuleVersion(), moduleHost.getMinimumModuleVersion());
                ModuleMetrics.recordLoadResult(ModuleMetrics.LOAD_RESULT_INCOMPATIBLE_VERSION);
                return null;
            }

            entryPoint.init(moduleHost);
            return entryPoint;
        } catch (ClassNotFoundException e) {
            Log.e(TAG, "Could not find class %", className, e);
            ModuleMetrics.recordLoadResult(ModuleMetrics.LOAD_RESULT_CLASS_NOT_FOUND_EXCEPTION);
        } catch (Exception e) {
            // No multi-catch below API level 19 for reflection exceptions.
            // This catches InstantiationException and IllegalAccessException.
            Log.e(TAG, "Could not instantiate class %", className, e);
            ModuleMetrics.recordLoadResult(ModuleMetrics.LOAD_RESULT_INSTANTIATION_EXCEPTION);
        }
        return null;
    }

    @Nullable
    private static Context getModuleContext(Context applicationContext, String packageName) {
        try {
            // The flags Context.CONTEXT_INCLUDE_CODE and Context.CONTEXT_IGNORE_SECURITY are
            // needed to be able to load classes via the classloader of the returned context.
            long createPackageContextStartTime = ModuleMetrics.now();
            Context moduleContext = applicationContext.createPackageContext(
                    packageName, Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY);
            ModuleMetrics.recordCreatePackageContextTime(createPackageContextStartTime);
            return moduleContext;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Could not create package context for %s", packageName, e);
            ModuleMetrics.recordLoadResult(
                    ModuleMetrics.LOAD_RESULT_PACKAGE_NAME_NOT_FOUND_EXCEPTION);
        }
        return null;
    }

    private static boolean isCompatible(ModuleHostImpl moduleHost, ModuleEntryPoint entryPoint) {
        return entryPoint.getModuleVersion() >= moduleHost.getMinimumModuleVersion()
                && moduleHost.getHostVersion() >= entryPoint.getMinimumHostVersion();
    }
}
