// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.process_launcher.ChildProcessCreationParams;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This class is responsible for allocating and managing connections to child
 * process services. These connections are in a pool (the services are defined
 * in the AndroidManifest.xml).
 */
public class ChildConnectionAllocator {
    private static final String TAG = "ChildConnAllocator";

    /** Listener that clients can use to get notified when connections get freed. */
    public interface Listener {
        void onConnectionFreed(
                ChildConnectionAllocator allocator, ChildProcessConnection connection);
    }

    /** Factory interface. Used by tests to specialize created connections. */
    @VisibleForTesting
    protected interface ConnectionFactory {
        ChildProcessConnection createConnection(Context context, ComponentName serviceName,
                boolean bindAsExternalService, Bundle serviceBundle,
                ChildProcessCreationParams creationParams,
                ChildProcessConnection.DeathCallback deathCallback);
    }

    /** Default implementation of the ConnectionFactory that creates actual connections. */
    private static class ConnectionFactoryImpl implements ConnectionFactory {
        @Override
        public ChildProcessConnection createConnection(Context context, ComponentName serviceName,
                boolean bindAsExternalService, Bundle serviceBundle,
                ChildProcessCreationParams creationParams,
                ChildProcessConnection.DeathCallback deathCallback) {
            return new ChildProcessConnection(context, serviceName, bindAsExternalService,
                    serviceBundle, creationParams, deathCallback);
        }
    }

    // Delay between the call to freeConnection and the connection actually beeing   freed.
    private static final long FREE_CONNECTION_DELAY_MILLIS = 1;

    // Connections to services. Indices of the array correspond to the service numbers.
    private final ChildProcessConnection[] mChildProcessConnections;

    private final String mPackageName;
    private final String mServiceClassName;
    private final boolean mBindAsExternalService;
    private final ChildProcessCreationParams mCreationParams;

    // The list of free (not bound) service indices.
    private final ArrayList<Integer> mFreeConnectionIndices;

    private final List<Listener> mListeners = new ArrayList<>();

    private ConnectionFactory mConnectionFactory = new ConnectionFactoryImpl();

    /**
     * Factory method that retrieves the service name and number of service from the
     * AndroidManifest.xml.
     */
    public static ChildConnectionAllocator create(Context context,
            ChildProcessCreationParams creationParams, String packageName,
            String serviceClassNameManifestKey, String numChildServicesManifestKey,
            boolean bindAsExternalService) {
        String serviceClassName = null;
        int numServices = -1;
        PackageManager packageManager = context.getPackageManager();
        try {
            ApplicationInfo appInfo =
                    packageManager.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
            if (appInfo.metaData != null) {
                serviceClassName = appInfo.metaData.getString(serviceClassNameManifestKey);
                numServices = appInfo.metaData.getInt(numChildServicesManifestKey, -1);
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Could not get application info.");
        }

        if (numServices < 0) {
            throw new RuntimeException("Illegal meta data value for number of child services");
        }

        // Check that the service exists.
        try {
            // PackageManager#getServiceInfo() throws an exception if the service does not
            // exist.
            packageManager.getServiceInfo(
                    new ComponentName(packageName, serviceClassName + "0"), 0);
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Illegal meta data value: the child service doesn't exist");
        }

        return new ChildConnectionAllocator(
                creationParams, packageName, serviceClassName, bindAsExternalService, numServices);
    }

    // TODO(jcivelli): remove this method once crbug.com/693484 has been addressed.
    static int getNumberOfServices(
            Context context, String packageName, String numChildServicesManifestKey) {
        int numServices = -1;
        try {
            PackageManager packageManager = context.getPackageManager();
            ApplicationInfo appInfo =
                    packageManager.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
            if (appInfo.metaData != null) {
                numServices = appInfo.metaData.getInt(numChildServicesManifestKey, -1);
            }
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Could not get application info", e);
        }
        if (numServices < 0) {
            throw new RuntimeException("Illegal meta data value for number of child services");
        }
        return numServices;
    }

    /**
     * Factory method used with some tests to create an allocator with values passed in directly
     * instead of being retrieved from the AndroidManifest.xml.
     */
    @VisibleForTesting
    public static ChildConnectionAllocator createForTest(ChildProcessCreationParams creationParams,
            String packageName, String serviceClassName, int serviceCount,
            boolean bindAsExternalService) {
        return new ChildConnectionAllocator(
                creationParams, packageName, serviceClassName, bindAsExternalService, serviceCount);
    }

    private ChildConnectionAllocator(ChildProcessCreationParams creationParams, String packageName,
            String serviceClassName, boolean bindAsExternalService, int numChildServices) {
        mCreationParams = creationParams;
        mPackageName = packageName;
        mServiceClassName = serviceClassName;
        mBindAsExternalService = bindAsExternalService;
        mChildProcessConnections = new ChildProcessConnection[numChildServices];
        mFreeConnectionIndices = new ArrayList<Integer>(numChildServices);
        for (int i = 0; i < numChildServices; i++) {
            mFreeConnectionIndices.add(i);
        }
    }

    /** @return a connection ready to be bound, or null if there are no free slots. */
    public ChildProcessConnection allocate(Context context, Bundle serviceBundle,
            final ChildProcessConnection.DeathCallback deathCallback) {
        assert LauncherThread.runningOnLauncherThread();
        if (mFreeConnectionIndices.isEmpty()) {
            Log.d(TAG, "Ran out of services to allocate.");
            return null;
        }
        int slot = mFreeConnectionIndices.remove(0);
        assert mChildProcessConnections[slot] == null;
        ComponentName serviceName = new ComponentName(mPackageName, mServiceClassName + slot);

        ChildProcessConnection.DeathCallback deathCallbackWrapper =
                new ChildProcessConnection.DeathCallback() {
                    @Override
                    public void onChildProcessDied(final ChildProcessConnection connection) {
                        assert LauncherThread.runningOnLauncherThread();

                        // Forward the call to the provided callback if any.
                        if (deathCallback != null) {
                            deathCallback.onChildProcessDied(connection);
                        }

                        // Freeing a service should be delayed. This is so that we avoid immediately
                        // reusing the freed service (see http://crbug.com/164069): the framework
                        // might keep a service process alive when it's been unbound for a short
                        // time. If a new connection to the same service is bound at that point, the
                        // process is reused and bad things happen (mostly static variables are set
                        // when we don't expect them to).
                        LauncherThread.postDelayed(new Runnable() {
                            @Override
                            public void run() {
                                free(connection);
                            }
                        }, FREE_CONNECTION_DELAY_MILLIS);
                    }
                };

        mChildProcessConnections[slot] = mConnectionFactory.createConnection(context, serviceName,
                mBindAsExternalService, serviceBundle, mCreationParams, deathCallbackWrapper);
        Log.d(TAG, "Allocator allocated a connection, name: %s, slot: %d", mServiceClassName, slot);
        return mChildProcessConnections[slot];
    }

    /** Frees a connection and notifies listeners. */
    private void free(ChildProcessConnection connection) {
        assert LauncherThread.runningOnLauncherThread();

        // mChildProcessConnections is relatively short (20 items at max at this point).
        // We are better of iterating than caching in a map.
        int slot = Arrays.asList(mChildProcessConnections).indexOf(connection);
        if (slot == -1) {
            Log.e(TAG, "Unable to find connection to free.");
            assert false;
        } else {
            mChildProcessConnections[slot] = null;
            assert !mFreeConnectionIndices.contains(slot);
            mFreeConnectionIndices.add(slot);
            Log.d(TAG, "Allocator freed a connection, name: %s, slot: %d", mServiceClassName, slot);
        }
        // Copy the listeners list so listeners can unregister themselves while we are iterating.
        List<Listener> listeners = new ArrayList<>(mListeners);
        for (Listener listener : listeners) {
            listener.onConnectionFreed(this, connection);
        }
    }

    public String getPackageName() {
        return mPackageName;
    }

    public boolean anyConnectionAllocated() {
        return mFreeConnectionIndices.size() < mChildProcessConnections.length;
    }

    public boolean isFreeConnectionAvailable() {
        assert LauncherThread.runningOnLauncherThread();
        return !mFreeConnectionIndices.isEmpty();
    }

    public int getNumberOfServices() {
        return mChildProcessConnections.length;
    }

    public void addListener(Listener listener) {
        assert !mListeners.contains(listener);
        mListeners.add(listener);
    }

    public void removeListener(Listener listener) {
        boolean removed = mListeners.remove(listener);
        assert removed;
    }

    @VisibleForTesting
    void setConnectionFactoryForTesting(ConnectionFactory connectionFactory) {
        mConnectionFactory = connectionFactory;
    }

    /** @return the count of connections managed by the allocator */
    @VisibleForTesting
    int allocatedConnectionsCountForTesting() {
        assert LauncherThread.runningOnLauncherThread();
        return mChildProcessConnections.length - mFreeConnectionIndices.size();
    }

    @VisibleForTesting
    ChildProcessConnection getChildProcessConnectionAtSlotForTesting(int slotNumber) {
        return mChildProcessConnections[slotNumber];
    }
}
