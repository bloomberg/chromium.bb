// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.content.app.LibraryLoader;
import org.chromium.content.common.CommandLine;
import org.chromium.content.common.ISandboxedProcessCallback;
import org.chromium.content.common.ISandboxedProcessService;

/**
 * This class provides the method to start/stop SandboxedProcess called by
 * native.
 */
@JNINamespace("content")
public class SandboxedProcessLauncher {
    private static String TAG = "SandboxedProcessLauncher";

    // The upper limit on the number of simultaneous service process instances supported.
    // This must not exceed total number of SandboxedProcessServiceX classes declared in
    // this package, and defined as services in the embedding application's manifest file.
    // (See {@link SandboxedProcessService} for more details on defining the services.)
    /* package */ static final int MAX_REGISTERED_SERVICES = 6;
    private static final SandboxedProcessConnection[] mConnections =
        new SandboxedProcessConnection[MAX_REGISTERED_SERVICES];
    // The list of free slots in mConnections.  When looking for a free connection,
    // the first index in that list should be used. When a connection is freed, its index
    // is added to the end of the list. This is so that we avoid immediately reusing a freed
    // connection (see bug crbug.com/164069): the framework might keep a service process alive
    // when it's been unbound for a short time.  If a connection to that same service is bound
    // at that point, the process is reused and bad things happen (mostly static variables are
    // set when we don't expect them to).
    // SHOULD BE ACCESSED WITH THE mConnections LOCK.
    private static final ArrayList<Integer> mFreeConnectionIndices =
        new ArrayList<Integer>(MAX_REGISTERED_SERVICES);
    static {
        for (int i = 0; i < MAX_REGISTERED_SERVICES; i++) {
            mFreeConnectionIndices.add(i);
        }
    }

    private static SandboxedProcessConnection allocateConnection(Context context) {
        SandboxedProcessConnection.DeathCallback deathCallback =
            new SandboxedProcessConnection.DeathCallback() {
                @Override
                public void onSandboxedProcessDied(int pid) {
                    stop(pid);
                }
            };
        synchronized (mConnections) {
            if (mFreeConnectionIndices.isEmpty()) {
                Log.w(TAG, "Ran out of sandboxed services.");
                return null;
            }
            int slot = mFreeConnectionIndices.remove(0);
            assert mConnections[slot] == null;
            mConnections[slot] = new SandboxedProcessConnection(context, slot, deathCallback);
            return mConnections[slot];
        }
    }

    private static SandboxedProcessConnection allocateBoundConnection(Context context,
            String[] commandLine) {
        SandboxedProcessConnection connection = allocateConnection(context);
        if (connection != null) {
            String libraryName = LibraryLoader.getLibraryToLoad();
            assert libraryName != null : "Attempting to launch a sandbox process without first "
                    + "calling LibraryLoader.setLibraryToLoad";
            connection.bind(libraryName, commandLine);
        }
        return connection;
    }

    private static void freeConnection(SandboxedProcessConnection connection) {
        if (connection == null) {
            return;
        }
        int slot = connection.getServiceNumber();
        synchronized (mConnections) {
            if (mConnections[slot] != connection) {
                int occupier = mConnections[slot] == null ?
                        -1 : mConnections[slot].getServiceNumber();
                Log.e(TAG, "Unable to find connection to free in slot: " + slot +
                        " already occupied by service: " + occupier);
                assert false;
            } else {
                mConnections[slot] = null;
                assert !mFreeConnectionIndices.contains(slot);
                mFreeConnectionIndices.add(slot);
            }
        }
    }

    public static int getNumberOfConnections() {
        synchronized (mConnections) {
            return mFreeConnectionIndices.size();
        }
    }

    // Represents an invalid process handle; same as base/process.h kNullProcessHandle.
    private static final int NULL_PROCESS_HANDLE = 0;

    // Map from pid to SandboxedService connection.
    private static Map<Integer, SandboxedProcessConnection> mServiceMap =
            new ConcurrentHashMap<Integer, SandboxedProcessConnection>();

    // A pre-allocated and pre-bound connection ready for connection setup, or null.
    static SandboxedProcessConnection mSpareConnection = null;

    /**
     * Returns the sandboxed process service interface for the given pid. This may be called on
     * any thread, but the caller must assume that the service can disconnect at any time. All
     * service calls should catch and handle android.os.RemoteException.
     *
     * @param pid The pid (process handle) of the service obtained from {@link #start}.
     * @return The ISandboxedProcessService or null if the service no longer exists.
     */
    public static ISandboxedProcessService getSandboxedService(int pid) {
        SandboxedProcessConnection connection = mServiceMap.get(pid);
        if (connection != null) {
            return connection.getService();
        }
        return null;
    }

    /**
     * Should be called early in startup so the work needed to spawn the sandboxed process can
     * be done in parallel to other startup work. Must not be called on the UI thread.
     * @param context the application context used for the connection.
     */
    public static synchronized void warmUp(Context context) {
        assert !ThreadUtils.runningOnUiThread();
        if (mSpareConnection == null) {
            mSpareConnection = allocateBoundConnection(context, null);
        }
    }

    /**
     * Spawns and connects to a sandboxed process. May be called on any thread. It will not
     * block, but will instead callback to {@link #nativeOnSandboxedProcessStarted} when the
     * connection is established. Note this callback will not necessarily be from the same thread
     * (currently it always comes from the main thread).
     *
     * @param context Context used to obtain the application context.
     * @param commandLine The sandboxed process command line argv.
     * @param file_ids The ID that should be used when mapping files in the created process.
     * @param file_fds The file descriptors that should be mapped in the created process.
     * @param file_auto_close Whether the file descriptors should be closed once they were passed to
     * the created process.
     * @param clientContext Arbitrary parameter used by the client to distinguish this connection.
     */
    @CalledByNative
    static void start(
            Context context,
            final String[] commandLine,
            int[] fileIds,
            int[] fileFds,
            boolean[] fileAutoClose,
            final int clientContext) {
        assert fileIds.length == fileFds.length && fileFds.length == fileAutoClose.length;
        FileDescriptorInfo[] filesToBeMapped = new FileDescriptorInfo[fileFds.length];
        for (int i = 0; i < fileFds.length; i++) {
            filesToBeMapped[i] =
                    new FileDescriptorInfo(fileIds[i], fileFds[i], fileAutoClose[i]);
        }
        assert clientContext != 0;
        SandboxedProcessConnection allocatedConnection;
        synchronized (SandboxedProcessLauncher.class) {
            allocatedConnection = mSpareConnection;
            mSpareConnection = null;
        }
        if (allocatedConnection == null) {
            allocatedConnection = allocateBoundConnection(context, commandLine);
            if (allocatedConnection == null) {
                // Notify the native code so it can free the heap allocated callback.
                nativeOnSandboxedProcessStarted(clientContext, 0);
                return;
            }
        }
        final SandboxedProcessConnection connection = allocatedConnection;
        Log.d(TAG, "Setting up connection to process: slot=" + connection.getServiceNumber());
        // Note: This runnable will be executed when the sandboxed connection is setup.
        final Runnable onConnect = new Runnable() {
            @Override
            public void run() {
                final int pid = connection.getPid();
                Log.d(TAG, "on connect callback, pid=" + pid + " context=" + clientContext);
                if (pid != NULL_PROCESS_HANDLE) {
                    mServiceMap.put(pid, connection);
                } else {
                    freeConnection(connection);
                }
                nativeOnSandboxedProcessStarted(clientContext, pid);
            }
        };
        connection.setupConnection(commandLine, filesToBeMapped, createCallback(), onConnect);
    }

    /**
     * Terminates a sandboxed process. This may be called from any thread.
     *
     * @param pid The pid (process handle) of the service connection obtained from {@link #start}.
     */
    @CalledByNative
    static void stop(int pid) {
        Log.d(TAG, "stopping sandboxed connection: pid=" + pid);

        SandboxedProcessConnection connection = mServiceMap.remove(pid);
        if (connection == null) {
            Log.w(TAG, "Tried to stop non-existent connection to pid: " + pid);
            return;
        }
        connection.unbind();
        freeConnection(connection);
    }

    /**
     * Bind a sandboxed process as a high priority process so that it has the same
     * priority as the main process. This can be used for the foreground renderer
     * process to distinguish it from the the background renderer process.
     *
     * @param pid The process handle of the service connection obtained from {@link #start}.
     */
    static void bindAsHighPriority(int pid) {
        SandboxedProcessConnection connection = mServiceMap.get(pid);
        if (connection == null) {
            Log.w(TAG, "Tried to bind a non-existent connection to pid: " + pid);
            return;
        }
        connection.bindHighPriority();
    }

    /**
     * Unbind a high priority process which is bound by {@link #bindAsHighPriority}.
     *
     * @param pid The process handle of the service obtained from {@link #start}.
     */
    static void unbindAsHighPriority(int pid) {
        SandboxedProcessConnection connection = mServiceMap.get(pid);
        if (connection == null) {
            Log.w(TAG, "Tried to unbind non-existent connection to pid: " + pid);
            return;
        }
        connection.unbindHighPriority(false);
    }

    static void establishSurfacePeer(
            int pid, int type, Surface surface, int primaryID, int secondaryID) {
        Log.d(TAG, "establishSurfaceTexturePeer: pid = " + pid + ", " +
              "type = " + type + ", " +
              "primaryID = " + primaryID + ", " +
              "secondaryID = " + secondaryID);
        ISandboxedProcessService service = SandboxedProcessLauncher.getSandboxedService(pid);
        if (service == null) {
            Log.e(TAG, "Unable to get SandboxedProcessService from pid.");
            return;
        }
        try {
            service.setSurface(type, surface, primaryID, secondaryID);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call setSurface: " + e);
        }
    }

    /**
     * This implementation is used to receive callbacks from the remote service.
     */
    private static ISandboxedProcessCallback createCallback() {
        return new ISandboxedProcessCallback.Stub() {
            /**
             * This is called by the remote service regularly to tell us about
             * new values.  Note that IPC calls are dispatched through a thread
             * pool running in each process, so the code executing here will
             * NOT be running in our main thread -- so, to update the UI, we need
             * to use a Handler.
             */
            public void establishSurfacePeer(
                    int pid, int type, Surface surface, int primaryID, int secondaryID) {
                SandboxedProcessLauncher.establishSurfacePeer(pid, type, surface,
                        primaryID, secondaryID);
                // The SandboxProcessService now holds a reference to the
                // Surface's resources, so we release our reference to it now to
                // avoid waiting for the finalizer to get around to it.
                if (surface != null) {
                    surface.release();
                }
            }
        };
    };

    private static native void nativeOnSandboxedProcessStarted(int clientContext, int pid);
}
