// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;

import com.google.protobuf.nano.MessageNano;

import org.chromium.base.ThreadUtils;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.protocol.EntitySpecifics;

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
        checkFakeServerInitialized(
                "useFakeServer must be called before calling deleteFakeServer.");
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
        checkFakeServerInitialized(
                "useFakeServer must be called before data verification.");
        return nativeVerifyEntityCountByTypeAndName(mNativeFakeServerHelperAndroid,
            sNativeFakeServer, count, modelType.toString(), name);
    }

    /**
     * Injects an entity into the fake Sync server. This method only works for entities that will
     * eventually contain a unique client tag (e.g., preferences, typed URLs).
     *
     * @param name the human-readable name for the entity. This value will be used for the
     *             SyncEntity.name value
     * @param entitySpecifics the EntitySpecifics proto that represents the entity to inject
     */
    public void injectUniqueClientEntity(String name, EntitySpecifics entitySpecifics) {
        checkFakeServerInitialized("useFakeServer must be called before data injection.");
        // The protocol buffer is serialized as a byte array because it can be easily deserialized
        // from this format in native code.
        nativeInjectUniqueClientEntity(mNativeFakeServerHelperAndroid, sNativeFakeServer, name,
                MessageNano.toByteArray(entitySpecifics));
    }

    /**
     * Injects a bookmark into the fake Sync server.
     *
     * @param title the title of the bookmark to inject
     * @param url the URL of the bookmark to inject. This String will be passed to the native GURL
     *            class, so it must be a valid URL under its definition.
     * @param parentId the ID of the desired parent bookmark folder
     */
    public void injectBookmarkEntity(String title, String url, String parentId) {
        checkFakeServerInitialized("useFakeServer must be called before data injection.");
        nativeInjectBookmarkEntity(mNativeFakeServerHelperAndroid, sNativeFakeServer, title, url,
                parentId);
    }

    /**
     * Returns the ID of the Bookmark Bar. This value is to be used in conjunction with
     * injectBookmarkEntity.
     *
     * @return the opaque ID of the bookmark bar entity stored in the server
     */
    public String getBookmarkBarFolderId() {
        checkFakeServerInitialized("useFakeServer must be called before access");
        return nativeGetBookmarkBarFolderId(mNativeFakeServerHelperAndroid, sNativeFakeServer);
    }

    private static void checkFakeServerInitialized(String failureMessage) {
        if (sNativeFakeServer == 0L) {
            throw new IllegalStateException(failureMessage);
        }
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
    private native void nativeInjectUniqueClientEntity(
            long nativeFakeServerHelperAndroid, long nativeFakeServer, String name,
            byte[] serializedEntitySpecifics);
    private native void nativeInjectBookmarkEntity(
            long nativeFakeServerHelperAndroid, long nativeFakeServer, String title, String url,
            String parentId);
    private native String nativeGetBookmarkBarFolderId(
            long nativeFakeServerHelperAndroid, long nativeFakeServer);
}
