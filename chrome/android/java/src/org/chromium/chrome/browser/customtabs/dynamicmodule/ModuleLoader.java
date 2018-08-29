// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.Process;
import android.support.annotation.Nullable;

import org.chromium.base.AsyncTask;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.crash.CrashKeyIndex;
import org.chromium.chrome.browser.crash.CrashKeys;

/**
 * Dynamically loads a module from another apk.
 */
public class ModuleLoader {
    private static final String TAG = "ModuleLoader";

    /** Specifies the module package name and entry point class name. */
    private final ComponentName mComponentName;
    private final String mModuleId;
    private int mModuleUseCount;
    @Nullable
    private ModuleEntryPoint mModuleEntryPoint;

    /**
     * Instantiates a new {@link ModuleLoader}.
     * @param componentName Specifies the module package name and entry point class name.
     */
    public ModuleLoader(ComponentName componentName) {
        mComponentName = componentName;
        String packageName = componentName.getPackageName();
        String version = "";
        try {
            version = ContextUtils.getApplicationContext()
                              .getPackageManager()
                              .getPackageInfo(packageName, 0)
                              .versionName;
        } catch (PackageManager.NameNotFoundException ignored) {
            // Ignore the exception. Failure to find the package name will be handled in
            // getModuleContext() below.
        }
        mModuleId = packageName + ":" + version;
    }

    public ComponentName getComponentName() {
        return mComponentName;
    }

    /**
     * If the module is not loaded yet, dynamically loads the module entry point class. If
     * successful, the callback will receive a {@link ModuleEntryPoint} asynchronously. If the
     * module fails to load, the callback will receive null. If the module was already loaded and a
     * reference to it is still held, the callback will synchronously receive a
     * {@link ModuleEntryPoint}.
     *
     * @param callback The callback to receive the result.
     * @return If the module is not loaded yet, an {@link AsyncTask} will be used to load it and
     *     a {@link Runnable} returned to the caller for cancelling the task if necessary. If the
     *     module was already loaded, null is returned.
     */
    @Nullable
    public Runnable loadModule(Callback<ModuleEntryPoint> callback) {
        if (mModuleEntryPoint != null) {
            mModuleUseCount++;
            ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.SUCCESS_CACHED);
            callback.onResult(mModuleEntryPoint);
            return null;
        }

        Context moduleContext = getModuleContext(mComponentName.getPackageName());
        if (moduleContext == null) {
            callback.onResult(null);
            return null;
        }

        // TODO(crbug.com/864520): Define and return a CancellablePromise instead.
        final LoadClassTask task = new LoadClassTask(moduleContext, callback);
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        return new Runnable() {
            @Override
            public void run() {
                task.cancel(false);
            }
        };
    }

    public void maybeUnloadModule() {
        if (mModuleEntryPoint == null) return;
        mModuleUseCount--;
        if (mModuleUseCount == 0) {
            mModuleEntryPoint.onDestroy();
            CrashKeys.getInstance().set(CrashKeyIndex.ACTIVE_DYNAMIC_MODULE, null);
            mModuleEntryPoint = null;
        }
    }

    /**
     * A task for loading the module entry point class on a background thread.
     */
    private class LoadClassTask extends AsyncTask<Class<?>> {
        private final Context mModuleContext;
        private final Callback<ModuleEntryPoint> mCallback;

        /**
         * Constructs the task.
         * @param moduleContext The context for the package to load the class from.
         * @param callback The callback to receive the result of the task. If there was a problem
         *     the result will be null.
         */
        LoadClassTask(Context moduleContext, Callback<ModuleEntryPoint> callback) {
            mModuleContext = moduleContext;
            mCallback = callback;
        }

        @Override
        @Nullable
        protected Class<?> doInBackground() {
            int oldPriority = Process.getThreadPriority(0);
            try {
                // We don't want to block the UI thread, but we don't want to be really slow either.
                // The AsyncTask class sets the thread priority quite low
                // (THREAD_PRIORITY_BACKGROUND) and does not distinguish between user-visible
                // user-invisible tasks.
                // TODO(crbug.com/863457): Replace this with something like a task trait that
                // influences priority once we have a task scheduler in Java.
                Process.setThreadPriority(Process.THREAD_PRIORITY_DEFAULT);

                long entryPointLoadClassStartTime = ModuleMetrics.now();
                Class<?> clazz =
                        mModuleContext.getClassLoader().loadClass(mComponentName.getClassName());
                ModuleMetrics.recordLoadClassTime(entryPointLoadClassStartTime);
                return clazz;
            } catch (ClassNotFoundException e) {
                Log.e(TAG, "Could not find class %s", mComponentName.getClassName(), e);
                ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.CLASS_NOT_FOUND_EXCEPTION);
            } finally {
                Process.setThreadPriority(oldPriority);
            }
            return null;
        }

        @Override
        protected void onPostExecute(@Nullable Class<?> clazz) {
            if (clazz == null) {
                mCallback.onResult(null);
                return;
            }

            try {
                long entryPointNewInstanceStartTime = ModuleMetrics.now();
                IBinder binder = (IBinder) clazz.newInstance();
                ModuleMetrics.recordEntryPointNewInstanceTime(entryPointNewInstanceStartTime);

                ModuleHostImpl moduleHost =
                        new ModuleHostImpl(ContextUtils.getApplicationContext(), mModuleContext);
                ModuleEntryPoint entryPoint =
                        new ModuleEntryPoint(IModuleEntryPoint.Stub.asInterface(binder));

                if (!isCompatible(moduleHost, entryPoint)) {
                    Log.w(TAG,
                            "Incompatible module due to version mismatch: host version %s, "
                                    + "minimum required host version %s, entry point version %s, "
                                    + "minimum required entry point version %s.",
                            moduleHost.getHostVersion(), entryPoint.getMinimumHostVersion(),
                            entryPoint.getModuleVersion(), moduleHost.getMinimumModuleVersion());
                    ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.INCOMPATIBLE_VERSION);
                    mCallback.onResult(null);
                    return;
                }

                CrashKeys crashKeys = CrashKeys.getInstance();
                crashKeys.set(CrashKeyIndex.LOADED_DYNAMIC_MODULE, mModuleId);
                crashKeys.set(CrashKeyIndex.ACTIVE_DYNAMIC_MODULE, mModuleId);

                entryPoint.init(moduleHost);
                ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.SUCCESS_NEW);
                mModuleEntryPoint = entryPoint;
                mModuleUseCount = 1;
                mCallback.onResult(entryPoint);
                return;
            } catch (Exception e) {
                // No multi-catch below API level 19 for reflection exceptions.
                // This catches InstantiationException and IllegalAccessException.
                Log.e(TAG, "Could not instantiate class %s", mComponentName.getClassName(), e);
                ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.INSTANTIATION_EXCEPTION);
            }
            mCallback.onResult(null);
        }
    }

    @Nullable
    private static Context getModuleContext(String packageName) {
        try {
            // The flags Context.CONTEXT_INCLUDE_CODE and Context.CONTEXT_IGNORE_SECURITY are
            // needed to be able to load classes via the classloader of the returned context.
            long createPackageContextStartTime = ModuleMetrics.now();
            Context moduleContext = ContextUtils.getApplicationContext().createPackageContext(
                    packageName, Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY);
            ModuleMetrics.recordCreatePackageContextTime(createPackageContextStartTime);
            return moduleContext;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Could not create package context for %s", packageName, e);
            ModuleMetrics.recordLoadResult(
                    ModuleMetrics.LoadResult.PACKAGE_NAME_NOT_FOUND_EXCEPTION);
        }
        return null;
    }

    private static boolean isCompatible(ModuleHostImpl moduleHost, ModuleEntryPoint entryPoint) {
        return entryPoint.getModuleVersion() >= moduleHost.getMinimumModuleVersion()
                && moduleHost.getHostVersion() >= entryPoint.getMinimumHostVersion();
    }
}
