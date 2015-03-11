// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.concurrent.Callable;

/**
 * Assists in Java interaction the native Sync FakeServer.
 */
public class FakeServerHelper {
    // Lazily-instantiated singleton FakeServerHelper.
    private static FakeServerHelper sFakeServerHelper;

    // Pointer value for the FakeServer. This pointer is not owned by native
    // code, so it must be stored here for future deletion.
    private static long sNativeFakeServer = 0L;

    // The pointer to the native object called here.
    private final long mNativeFakeServerHelperAndroid;

    // Accesses the singleton FakeServerHelper. There is at most one instance created per
    // application lifetime, so no deletion mechanism is provided for the native object.
    public static FakeServerHelper get() {
        ThreadUtils.assertOnUiThread();
        if (sFakeServerHelper == null) {
            sFakeServerHelper = new FakeServerHelper();
        }
        return sFakeServerHelper;
    }

    private FakeServerHelper() {
        mNativeFakeServerHelperAndroid = nativeInit();
    }

    /**
     * Creates and configures FakeServer.
     *
     * Each call to this method should be accompanied by a later call to deleteFakeServer to avoid
     * a memory leak.
     */
    public static void useFakeServer(final Context context) {
        if (sNativeFakeServer != 0L) {
            throw new IllegalStateException(
                    "deleteFakeServer must be called before calling useFakeServer again.");
        }

        sNativeFakeServer = ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Long>() {
            @Override
            public Long call() {
                FakeServerHelper fakeServerHelper = FakeServerHelper.get();
                long nativeFakeServer = fakeServerHelper.createFakeServer();
                long resources = fakeServerHelper.createNetworkResources(nativeFakeServer);
                ProfileSyncService.get(context).overrideNetworkResourcesForTest(resources);

                return nativeFakeServer;
            }
        });
    }

    /**
     * Deletes the existing FakeServer.
     */
    public static void deleteFakeServer() {
        if (sNativeFakeServer == 0L) {
            throw new IllegalStateException(
                    "useFakeServer must be called before calling deleteFakeServer.");
        }

        ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Void>() {
            @Override
            public Void call() {
                FakeServerHelper.get().deleteFakeServer(sNativeFakeServer);
                sNativeFakeServer = 0L;
                return null;
            }
        });
    }

    /**
     * Creates a native FakeServer object and returns its pointer. This pointer is owned by the
     * Java caller.
     *
     * @return the FakeServer pointer
     */
    public long createFakeServer() {
        return nativeCreateFakeServer(mNativeFakeServerHelperAndroid);
    }

    /**
     * Creates a native NetworkResources object. This pointer is owned by the Java caller, but
     * ownership is transferred as part of ProfileSyncService.overrideNetworkResourcesForTest.
     *
     * @param nativeFakeServer pointer to a native FakeServer object.
     * @return the NetworkResources pointer
     */
    public long createNetworkResources(long nativeFakeServer) {
        return nativeCreateNetworkResources(mNativeFakeServerHelperAndroid, nativeFakeServer);
    }

    /**
     * Deletes a native FakeServer.
     *
     * @param nativeFakeServer the pointer to be deleted
     */
    public void deleteFakeServer(long nativeFakeServer) {
        nativeDeleteFakeServer(mNativeFakeServerHelperAndroid, nativeFakeServer);
    }

    /**
     * Returns whether {@code count} entities exist on the fake Sync server with the given
     * {@code modelType} and {@code name}.
     *
     * @param count the number of fake server entities to verify
     * @param modelType the model type of entities to verify
     * @param name the name of entities to verify
     *
     * @return whether the number of specified entities exist
     */
    public boolean verifyEntityCountByTypeAndName(int count, ModelType modelType, String name) {
        if (sNativeFakeServer == 0L) {
            throw new IllegalStateException(
                "useFakeServer must be called before data verification.");
        }
        return nativeVerifyEntityCountByTypeAndName(mNativeFakeServerHelperAndroid,
            sNativeFakeServer, count, modelType.toString(), name);
    }

    /**
     * Injects a typed URL entity into the fake Sync server.
     *
     * @param url the URL of the entity. This will also be used as the entity's name.
     */
    // TODO(pvalenzuela): Generalize this method to injectEntity by serializing an EntitySpecifics
    // protocol buffer and passing that to the native code.
    public void injectTypedUrl(String url) {
        if (sNativeFakeServer == 0L) {
            throw new IllegalStateException(
                "useFakeServer must be called before data injection.");
        }
        nativeInjectTypedUrl(mNativeFakeServerHelperAndroid, sNativeFakeServer, url);
    }

    // Native methods.
    private native long nativeInit();
    private native long nativeCreateFakeServer(long nativeFakeServerHelperAndroid);
    private native long nativeCreateNetworkResources(
            long nativeFakeServerHelperAndroid, long nativeFakeServer);
    private native void nativeDeleteFakeServer(
            long nativeFakeServerHelperAndroid, long nativeFakeServer);
    private native boolean nativeVerifyEntityCountByTypeAndName(
            long nativeFakeServerHelperAndroid, long nativeFakeServer, int count, String modelType,
            String name);
    private native void nativeInjectTypedUrl(
            long nativeFakeServerHelperAndroid, long nativeFakeServer, String url);
}
