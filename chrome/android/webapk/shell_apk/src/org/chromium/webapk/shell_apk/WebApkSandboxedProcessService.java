// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Child process service hosted by WebAPKs. This class uses Chrome's ClassLoader to create a
 * {@link ChildProcessServiceImpl} object which loads Chrome's native libraries, initializes JNI
 * and creates the renderer.
 */
public class WebApkSandboxedProcessService extends Service {
    // Note: the {@link CHILD_PROCESS_SERVICE_IMPL_CLASS_NAME} must sync with the class name
    // of Chrome's {@link ChildProcessServiceImpl}.
    private static final String CHILD_PROCESS_SERVICE_IMPL_CLASS_NAME =
            "org.chromium.content.app.ChildProcessServiceImpl";
    private static final String TAG = "cr_WebApkSandboxedProcessService";

    private Class<?> mChildProcessServiceImplClass;
    private Object mChildProcessServiceImplInstance;

    @Override
    public void onCreate() {
        super.onCreate();

        try {
            Context hostBrowserContext =
                    WebApkUtils.getHostBrowserContext(getApplicationContext());
            ClassLoader classLoader = hostBrowserContext.getClassLoader();
            mChildProcessServiceImplClass =
                    classLoader.loadClass(CHILD_PROCESS_SERVICE_IMPL_CLASS_NAME);
            mChildProcessServiceImplInstance = mChildProcessServiceImplClass.newInstance();

            Method createMethod = mChildProcessServiceImplClass.getMethod("create",
                    Context.class, Context.class);
            createMethod.invoke(mChildProcessServiceImplInstance, getApplicationContext(),
                    hostBrowserContext);
        } catch (Exception e) {
            Log.v(TAG, "Unable to create a ChildProcessServiceImpl for the WebAPK.", e);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        // We call stopSelf() to request that this service be stopped as soon as the client
        // unbinds. Otherwise the system may keep it around and available for a reconnect. The
        // child processes do not currently support reconnect; they must be initialized from
        // scratch every time.
        stopSelf();
        try {
            Method bindMethod =
                    mChildProcessServiceImplClass.getMethod("bind", Intent.class, int.class);
            int hostBrowserUid = WebApkUtils.getHostBrowserUid(this);
            assert hostBrowserUid >= 0;
            return (IBinder) bindMethod.invoke(
                    mChildProcessServiceImplInstance, intent, hostBrowserUid);
        } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        try {
            Method destroyMethod = mChildProcessServiceImplClass.getMethod("destroy");
            destroyMethod.invoke(mChildProcessServiceImplInstance);
        } catch (Exception e) {
            Log.v(TAG, "Unable to destroy the WebApkSandboxedProcessService.", e);
        }
    }
}
