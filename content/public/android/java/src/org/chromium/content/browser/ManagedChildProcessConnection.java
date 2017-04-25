// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.process_launcher.ChildProcessCreationParams;

import javax.annotation.concurrent.GuardedBy;

/**
 * ManagedChildProcessConnection is a connection to a child service that can hold several bindings
 * to the service so it can be more or less agressively protected against OOM.
 */
public class ManagedChildProcessConnection extends BaseChildProcessConnection {
    private static final String TAG = "ManChildProcessConn";

    public static final Factory FACTORY = new BaseChildProcessConnection.Factory() {
        @Override
        public BaseChildProcessConnection create(Context context, int number, boolean sandboxed,
                DeathCallback deathCallback, String serviceClassName,
                Bundle childProcessCommonParameters, ChildProcessCreationParams creationParams) {
            return new ManagedChildProcessConnection(context, number, sandboxed, deathCallback,
                    serviceClassName, childProcessCommonParameters, creationParams);
        }
    };

    // Synchronization: While most internal flow occurs on the UI thread, the public API
    // (specifically start and stop) may be called from any thread, hence all entry point methods
    // into the class are synchronized on the lock to protect access to these members.
    private final Object mBindingLock = new Object();

    // Initial binding protects the newly spawned process from being killed before it is put to use,
    // it is maintained between calls to start() and removeInitialBinding().
    @GuardedBy("mBindingLock")
    private final ChildServiceConnection mInitialBinding;

    // Strong binding will make the service priority equal to the priority of the activity. We want
    // the OS to be able to kill background renderers as it kills other background apps, so strong
    // bindings are maintained only for services that are active at the moment (between
    // addStrongBinding() and removeStrongBinding()).
    @GuardedBy("mBindingLock")
    private final ChildServiceConnection mStrongBinding;

    // Low priority binding maintained in the entire lifetime of the connection, i.e. between calls
    // to start() and stop().
    @GuardedBy("mBindingLock")
    private final ChildServiceConnection mWaivedBinding;

    // Incremented on addStrongBinding(), decremented on removeStrongBinding().
    @GuardedBy("mBindingLock")
    private int mStrongBindingCount;

    // Moderate binding will make the service priority equal to the priority of a visible process
    // while the app is in the foreground. It will stay bound only while the app is in the
    // foreground to protect a background process from the system out-of-memory killer.
    @GuardedBy("mBindingLock")
    private final ChildServiceConnection mModerateBinding;

    @GuardedBy("mBindingLock")
    private boolean mWasOomProtectedOnUnbind;

    @VisibleForTesting
    ManagedChildProcessConnection(Context context, int number, boolean sandboxed,
            DeathCallback deathCallback, String serviceClassName,
            Bundle childProcessCommonParameters, ChildProcessCreationParams creationParams) {
        super(context, number, sandboxed, deathCallback, serviceClassName,
                childProcessCommonParameters, creationParams);

        int initialFlags = Context.BIND_AUTO_CREATE;
        int extraBindFlags = shouldBindAsExportedService() ? Context.BIND_EXTERNAL_SERVICE : 0;

        synchronized (mBindingLock) {
            mInitialBinding = createServiceConnection(initialFlags | extraBindFlags);
            mStrongBinding = createServiceConnection(
                    Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT | extraBindFlags);
            mWaivedBinding = createServiceConnection(
                    Context.BIND_AUTO_CREATE | Context.BIND_WAIVE_PRIORITY | extraBindFlags);
            mModerateBinding = createServiceConnection(Context.BIND_AUTO_CREATE | extraBindFlags);
        }
    }

    @Override
    protected boolean bind() {
        synchronized (mBindingLock) {
            if (!mInitialBinding.bind()) {
                return false;
            }
            mWaivedBinding.bind();
        }
        return true;
    }

    @Override
    public void unbind() {
        synchronized (mBindingLock) {
            if (!isBound()) {
                return;
            }
            mWasOomProtectedOnUnbind = isCurrentlyOomProtected();
            mInitialBinding.unbind();
            mStrongBinding.unbind();
            mWaivedBinding.unbind();
            mModerateBinding.unbind();
            mStrongBindingCount = 0;
        }
    }

    @GuardedBy("mBindingLock")
    private boolean isBound() {
        return mInitialBinding.isBound() || mStrongBinding.isBound() || mWaivedBinding.isBound()
                || mModerateBinding.isBound();
    }

    public boolean isInitialBindingBound() {
        synchronized (mBindingLock) {
            return mInitialBinding.isBound();
        }
    }

    public boolean isStrongBindingBound() {
        synchronized (mBindingLock) {
            return mStrongBinding.isBound();
        }
    }

    public void removeInitialBinding() {
        synchronized (mBindingLock) {
            mInitialBinding.unbind();
        }
    }

    public boolean isOomProtectedOrWasWhenDied() {
        // Call isConnected() outside of the synchronized block or we could deadlock.
        final boolean isConnected = isConnected();
        synchronized (mBindingLock) {
            if (isConnected) {
                return isCurrentlyOomProtected();
            }
            return mWasOomProtectedOnUnbind;
        }
    }

    @GuardedBy("mBindingLock")
    private boolean isCurrentlyOomProtected() {
        return mInitialBinding.isBound() || mStrongBinding.isBound();
    }

    public void dropOomBindings() {
        synchronized (mBindingLock) {
            mInitialBinding.unbind();

            mStrongBindingCount = 0;
            mStrongBinding.unbind();

            mModerateBinding.unbind();
        }
    }

    public void addStrongBinding() {
        // Call isConnected() outside of the synchronized block or we could deadlock.
        final boolean isConnected = isConnected();
        synchronized (mBindingLock) {
            if (!isConnected) {
                Log.w(TAG, "The connection is not bound for %d", getPid());
                return;
            }
            if (mStrongBindingCount == 0) {
                mStrongBinding.bind();
            }
            mStrongBindingCount++;
        }
    }

    public void removeStrongBinding() {
        // Call isConnected() outside of the synchronized block or we could deadlock.
        final boolean isConnected = isConnected();
        synchronized (mBindingLock) {
            if (!isConnected) {
                Log.w(TAG, "The connection is not bound for %d", getPid());
                return;
            }
            assert mStrongBindingCount > 0;
            mStrongBindingCount--;
            if (mStrongBindingCount == 0) {
                mStrongBinding.unbind();
            }
        }
    }

    public boolean isModerateBindingBound() {
        synchronized (mBindingLock) {
            return mModerateBinding.isBound();
        }
    }

    public void addModerateBinding() {
        // Call isConnected() outside of the synchronized block or we could deadlock.
        final boolean isConnected = isConnected();
        synchronized (mBindingLock) {
            if (!isConnected) {
                Log.w(TAG, "The connection is not bound for %d", getPid());
                return;
            }
            mModerateBinding.bind();
        }
    }

    public void removeModerateBinding() {
        // Call isConnected() outside of the synchronized block or we could deadlock.
        final boolean isConnected = isConnected();
        synchronized (mBindingLock) {
            if (!isConnected) {
                Log.w(TAG, "The connection is not bound for %d", getPid());
                return;
            }
            mModerateBinding.unbind();
        }
    }
}
