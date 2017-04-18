// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.content.app.PrivilegedProcessService;
import org.chromium.content.app.SandboxedProcessService;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;

/**
 * This class is responsible for allocating and managing connections to child
 * process services. These connections are in a pool (the services are defined
 * in the AndroidManifest.xml).
 */
public class ChildConnectionAllocator {
    private static final String TAG = "ChildConnAllocator";

    private static final String NUM_SANDBOXED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_SANDBOXED_SERVICES";
    private static final String NUM_PRIVILEGED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_PRIVILEGED_SERVICES";
    private static final String SANDBOXED_SERVICES_NAME_KEY =
            "org.chromium.content.browser.SANDBOXED_SERVICES_NAME";

    // Map from package name to ChildConnectionAllocator.
    private static Map<String, ChildConnectionAllocator> sSandboxedChildConnectionAllocatorMap;

    // Allocator used for non-sandboxed services.
    private static ChildConnectionAllocator sPrivilegedChildConnectionAllocator;

    // Used by test to override the default sandboxed service settings.
    private static int sSandboxedServicesCountForTesting = -1;
    private static String sSandboxedServicesNameForTesting;

    // Connections to services. Indices of the array correspond to the service numbers.
    private final ChildProcessConnection[] mChildProcessConnections;

    private final String mChildClassName;
    private final boolean mInSandbox;

    // The list of free (not bound) service indices.
    private final ArrayList<Integer> mFreeConnectionIndices;

    // Each Allocator keeps a queue for the pending spawn data. Once a connection is free, we
    // dequeue the pending spawn data from the same allocator as the connection.
    private final Queue<ChildSpawnData> mPendingSpawnQueue = new LinkedList<>();

    @SuppressFBWarnings("LI_LAZY_INIT_STATIC") // Method is single thread.
    public static ChildConnectionAllocator getAllocator(
            Context context, String packageName, boolean inSandbox) {
        assert LauncherThread.runningOnLauncherThread();
        if (!inSandbox) {
            if (sPrivilegedChildConnectionAllocator == null) {
                sPrivilegedChildConnectionAllocator = new ChildConnectionAllocator(false,
                        getNumberOfServices(context, false, packageName),
                        getClassNameOfService(context, false, packageName));
            }
            return sPrivilegedChildConnectionAllocator;
        }

        if (sSandboxedChildConnectionAllocatorMap == null) {
            sSandboxedChildConnectionAllocatorMap = new HashMap<String, ChildConnectionAllocator>();
        }
        if (!sSandboxedChildConnectionAllocatorMap.containsKey(packageName)) {
            Log.w(TAG,
                    "Create a new ChildConnectionAllocator with package name = %s,"
                            + " inSandbox = true",
                    packageName);
            sSandboxedChildConnectionAllocatorMap.put(packageName,
                    new ChildConnectionAllocator(true,
                            getNumberOfServices(context, true, packageName),
                            getClassNameOfService(context, true, packageName)));
        }
        return sSandboxedChildConnectionAllocatorMap.get(packageName);
        // TODO(pkotwicz|hanxi): Figure out when old allocators should be removed from
        // {@code sSandboxedChildConnectionAllocatorMap}.
    }

    private static String getClassNameOfService(
            Context context, boolean inSandbox, String packageName) {
        if (!inSandbox) {
            return PrivilegedProcessService.class.getName();
        }

        if (!TextUtils.isEmpty(sSandboxedServicesNameForTesting)) {
            return sSandboxedServicesNameForTesting;
        }

        String serviceName = null;
        try {
            PackageManager packageManager = context.getPackageManager();
            ApplicationInfo appInfo =
                    packageManager.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
            if (appInfo.metaData != null) {
                serviceName = appInfo.metaData.getString(SANDBOXED_SERVICES_NAME_KEY);
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Could not get application info.");
        }

        if (serviceName != null) {
            // Check that the service exists.
            try {
                PackageManager packageManager = context.getPackageManager();
                // PackageManager#getServiceInfo() throws an exception if the service does not
                // exist.
                packageManager.getServiceInfo(new ComponentName(packageName, serviceName + "0"), 0);
                return serviceName;
            } catch (PackageManager.NameNotFoundException e) {
                throw new RuntimeException(
                        "Illegal meta data value: the child service doesn't exist");
            }
        }
        return SandboxedProcessService.class.getName();
    }

    static int getNumberOfServices(Context context, boolean inSandbox, String packageName) {
        int numServices = -1;
        if (inSandbox && sSandboxedServicesCountForTesting != -1) {
            numServices = sSandboxedServicesCountForTesting;
        } else {
            try {
                PackageManager packageManager = context.getPackageManager();
                ApplicationInfo appInfo = packageManager.getApplicationInfo(
                        packageName, PackageManager.GET_META_DATA);
                if (appInfo.metaData != null) {
                    numServices = appInfo.metaData.getInt(
                            inSandbox ? NUM_SANDBOXED_SERVICES_KEY : NUM_PRIVILEGED_SERVICES_KEY,
                            -1);
                }
            } catch (PackageManager.NameNotFoundException e) {
                throw new RuntimeException("Could not get application info", e);
            }
        }
        if (numServices < 0) {
            throw new RuntimeException("Illegal meta data value for number of child services");
        }
        return numServices;
    }

    @VisibleForTesting
    public static void setSandboxServicesSettingsForTesting(int serviceCount, String serviceName) {
        sSandboxedServicesCountForTesting = serviceCount;
        sSandboxedServicesNameForTesting = serviceName;
    }

    private ChildConnectionAllocator(
            boolean inSandbox, int numChildServices, String serviceClassName) {
        mChildProcessConnections = new ChildProcessConnectionImpl[numChildServices];
        mFreeConnectionIndices = new ArrayList<Integer>(numChildServices);
        for (int i = 0; i < numChildServices; i++) {
            mFreeConnectionIndices.add(i);
        }
        mChildClassName = serviceClassName;
        mInSandbox = inSandbox;
    }

    // Allocates or enqueues. If there are no free slots, returns null and enqueues the spawn data.
    public ChildProcessConnection allocate(ChildSpawnData spawnData,
            ChildProcessConnection.DeathCallback deathCallback, Bundle childProcessCommonParameters,
            boolean queueIfNoSlotAvailable) {
        assert LauncherThread.runningOnLauncherThread();
        assert spawnData.isInSandbox() == mInSandbox;
        if (mFreeConnectionIndices.isEmpty()) {
            Log.d(TAG, "Ran out of services to allocate.");
            if (queueIfNoSlotAvailable) {
                mPendingSpawnQueue.add(spawnData);
            }
            return null;
        }
        int slot = mFreeConnectionIndices.remove(0);
        assert mChildProcessConnections[slot] == null;
        mChildProcessConnections[slot] = new ChildProcessConnectionImpl(spawnData.getContext(),
                slot, mInSandbox, deathCallback, mChildClassName, childProcessCommonParameters,
                spawnData.isAlwaysInForeground(), spawnData.getCreationParams());
        Log.d(TAG, "Allocator allocated a connection, sandbox: %b, slot: %d", mInSandbox, slot);
        return mChildProcessConnections[slot];
    }

    // Also return the first ChildSpawnData in the pending queue, if any.
    public ChildSpawnData free(ChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();
        int slot = connection.getServiceNumber();
        if (mChildProcessConnections[slot] != connection) {
            int occupier = mChildProcessConnections[slot] == null
                    ? -1
                    : mChildProcessConnections[slot].getServiceNumber();
            Log.e(TAG,
                    "Unable to find connection to free in slot: %d "
                            + "already occupied by service: %d",
                    slot, occupier);
            assert false;
        } else {
            mChildProcessConnections[slot] = null;
            assert !mFreeConnectionIndices.contains(slot);
            mFreeConnectionIndices.add(slot);
            Log.d(TAG, "Allocator freed a connection, sandbox: %b, slot: %d", mInSandbox, slot);
        }
        return mPendingSpawnQueue.poll();
    }

    public boolean isFreeConnectionAvailable() {
        assert LauncherThread.runningOnLauncherThread();
        return !mFreeConnectionIndices.isEmpty();
    }

    /** @return the count of connections managed by the allocator */
    @VisibleForTesting
    int allocatedConnectionsCountForTesting() {
        assert LauncherThread.runningOnLauncherThread();
        return mChildProcessConnections.length - mFreeConnectionIndices.size();
    }

    @VisibleForTesting
    ChildProcessConnection[] connectionArrayForTesting() {
        return mChildProcessConnections;
    }

    @VisibleForTesting
    void enqueuePendingQueueForTesting(ChildSpawnData spawnData) {
        assert LauncherThread.runningOnLauncherThread();
        mPendingSpawnQueue.add(spawnData);
    }

    @VisibleForTesting
    int pendingSpawnsCountForTesting() {
        assert LauncherThread.runningOnLauncherThread();
        return mPendingSpawnQueue.size();
    }
}
