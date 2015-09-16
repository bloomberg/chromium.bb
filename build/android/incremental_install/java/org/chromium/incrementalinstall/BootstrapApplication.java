// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.incrementalinstall;

import android.app.Application;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.util.Log;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.List;
import java.util.Map;

/**
 * An Application that replaces itself with another Application (as defined in
 * the "incremental-install-real-app" meta-data tag within the
 * AndroidManifest.xml). It loads the other application only after side-loading
 * its .so and .dex files from /data/local/tmp.
 */
public final class BootstrapApplication extends Application {
    private static final String TAG = "cr.incrementalinstall";
    private static final String MANAGED_DIR_PREFIX = "/data/local/tmp/incremental-app-";
    private static final String REAL_APP_META_DATA_NAME = "incremental-install-real-app";

    private ClassLoaderPatcher mClassLoaderPatcher;
    private Application mRealApplication;
    private Object mStashedProviderList;
    private Object mActivityThread;

    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        File incrementalRootDir = new File(MANAGED_DIR_PREFIX + context.getPackageName());
        File libDir = new File(incrementalRootDir, "lib");
        File dexDir = new File(incrementalRootDir, "dex");
        File installLockFile = new File(incrementalRootDir, "install.lock");
        File firstRunLockFile = new File(incrementalRootDir, "firstrun.lock");

        try {
            mActivityThread = Reflect.invokeMethod(Class.forName("android.app.ActivityThread"),
                    "currentActivityThread");
            mClassLoaderPatcher = new ClassLoaderPatcher(context);

            boolean isFirstRun = LockFile.installerLockExists(firstRunLockFile);
            if (isFirstRun) {
                if (mClassLoaderPatcher.mIsPrimaryProcess) {
                    // Wait for incremental_install.py to finish.
                    LockFile.waitForInstallerLock(installLockFile, 20 * 1000);
                } else {
                    // Wait for the browser process to create the optimized dex files
                    // (and for M+, copy the library files).
                    LockFile.waitForInstallerLock(firstRunLockFile, 30 * 1000);
                }
            }

            mClassLoaderPatcher.importNativeLibs(libDir);
            mClassLoaderPatcher.loadDexFiles(dexDir);

            if (isFirstRun && mClassLoaderPatcher.mIsPrimaryProcess) {
                LockFile.clearInstallerLock(firstRunLockFile);
            }

            // attachBaseContext() is called from ActivityThread#handleBindApplication() and
            // Application#mApplication is changed right after we return. Thus, we cannot swap
            // the Application instances until onCreate() is called.
            String realApplicationName = getRealApplicationName();
            Log.i(TAG, "Instantiating " + realApplicationName);
            mRealApplication =
                    (Application) Reflect.newInstance(Class.forName(realApplicationName));
            Reflect.invokeMethod(mRealApplication, "attachBaseContext", context);

            // Between attachBaseContext() and onCreate(), ActivityThread tries to instantiate
            // all ContentProviders. The ContentProviders break without the correct Application
            // class being installed, so temporarily pretend there are no providers, and then
            // instantiate them explicitly within onCreate().
            disableContentProviders();
            Log.i(TAG, "Waiting for onCreate");
        } catch (Exception e) {
            throw new RuntimeException("Incremental install failed.", e);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        try {
            Log.i(TAG, "onCreate() called. Swapping Application references");
            swapApplicationReferences();
            enableContentProviders();
            Log.i(TAG, "Calling onCreate");
            mRealApplication.onCreate();
        } catch (Exception e) {
            throw new RuntimeException("Incremental install failed.", e);
        }
    }

    /**
     * Returns the class name of the real Application class (recorded in the
     * AndroidManifest.xml)
     */
    private String getRealApplicationName() throws NameNotFoundException {
        ApplicationInfo appInfo = getPackageManager().getApplicationInfo(getPackageName(),
                PackageManager.GET_META_DATA);
        return appInfo.metaData.getString(REAL_APP_META_DATA_NAME);
    }

    /**
     * Nulls out ActivityThread.mBoundApplication.providers.
     */
    private void disableContentProviders() throws ReflectiveOperationException {
        Object data = Reflect.getField(mActivityThread, "mBoundApplication");
        mStashedProviderList = Reflect.getField(data, "providers");
        Reflect.setField(data, "providers", null);
    }

    /**
     * Restores the value of ActivityThread.mBoundApplication.providers, and invokes
     * ActivityThread#installContentProviders().
     */
    private void enableContentProviders() throws ReflectiveOperationException {
        Object data = Reflect.getField(mActivityThread, "mBoundApplication");
        Reflect.setField(data, "providers", mStashedProviderList);
        if (mStashedProviderList != null && mClassLoaderPatcher.mIsPrimaryProcess) {
            Log.i(TAG, "Instantiating content providers");
            Reflect.invokeMethod(mActivityThread, "installContentProviders", mRealApplication,
                    mStashedProviderList);
        }
        mStashedProviderList = null;
    }

    /**
     * Changes all fields within framework classes that have stored an reference to this
     * BootstrapApplication to instead store references to mRealApplication.
     * @throws NoSuchFieldException
     */
    @SuppressWarnings("unchecked")
    private void swapApplicationReferences() throws ReflectiveOperationException {
        if (Reflect.getField(mActivityThread, "mInitialApplication") == this) {
            Reflect.setField(mActivityThread, "mInitialApplication", mRealApplication);
        }

        List<Application> allApplications =
                (List<Application>) Reflect.getField(mActivityThread, "mAllApplications");
        for (int i = 0; i < allApplications.size(); i++) {
            if (allApplications.get(i) == this) {
                allApplications.set(i, mRealApplication);
            }
        }

        for (String fieldName : new String[] { "mPackages", "mResourcePackages" }) {
            Map<String, WeakReference<?>> packageMap =
                    (Map<String, WeakReference<?>>) Reflect.getField(mActivityThread, fieldName);
            for (Map.Entry<String, WeakReference<?>> entry : packageMap.entrySet()) {
                Object loadedApk = entry.getValue().get();
                if (loadedApk != null && Reflect.getField(loadedApk, "mApplication") == this) {
                    Reflect.setField(loadedApk, "mApplication", mRealApplication);
                    Reflect.setField(mRealApplication, "mLoadedApk", loadedApk);
                }
            }
        }
    }
}
