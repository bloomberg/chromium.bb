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
    private static final String TAG = "cr_WebApkSandboxedProcessService";

    // Note: the {@link WEBAPK_CHILD_PROCESS_SERVICE_IMPL_CLASS_NAME} must sync with the class name
    // of Chrome's {@link WebApkChildProcessServiceImpl}.
    private static final String WEBAPK_CHILD_PROCESS_SERVICE_IMPL_CLASS_NAME =
            "org.chromium.chrome.browser.WebApkChildProcessServiceImpl";

    private Class<?> mWebApkChildProcessServiceImplClass;
    private Object mWebApkChildProcessServiceImplInstance;

    @Override
    public void onCreate() {
        super.onCreate();

        try {
            Context hostBrowserContext =
                    WebApkUtils.getHostBrowserContext(getApplicationContext());
            ClassLoader classLoader = hostBrowserContext.getClassLoader();
            mWebApkChildProcessServiceImplClass =
                    classLoader.loadClass(WEBAPK_CHILD_PROCESS_SERVICE_IMPL_CLASS_NAME);
            mWebApkChildProcessServiceImplInstance =
                    mWebApkChildProcessServiceImplClass.newInstance();

            Method createMethod = mWebApkChildProcessServiceImplClass.getMethod(
                    "create", Context.class, Context.class);
            createMethod.invoke(mWebApkChildProcessServiceImplInstance, getApplicationContext(),
                    hostBrowserContext);
        } catch (Exception e) {
            Log.v(TAG, "Unable to create a WebApkChildProcessServiceImpl for the WebAPK.", e);
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
                    mWebApkChildProcessServiceImplClass.getMethod("bind", Intent.class, int.class);
            int hostBrowserUid = WebApkUtils.getHostBrowserUid(this);
            assert hostBrowserUid >= 0;
            return (IBinder) bindMethod.invoke(
                    mWebApkChildProcessServiceImplInstance, intent, hostBrowserUid);
        } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        try {
            Method destroyMethod = mWebApkChildProcessServiceImplClass.getMethod("destroy");
            destroyMethod.invoke(mWebApkChildProcessServiceImplInstance);
        } catch (Exception e) {
            Log.v(TAG, "Unable to destroy the WebApkSandboxedProcessService.", e);
        }
    }
}
