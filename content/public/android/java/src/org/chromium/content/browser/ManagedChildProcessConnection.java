// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.process_launcher.ChildProcessCreationParams;

/**
 * ManagedChildProcessConnection is a connection to a child service that can hold several bindings
 * to the service so it can be more or less agressively protected against OOM.
 * Accessed from the launcher thread. (but for isOomProtectedOrWasWhenDied()).
 */
public class ManagedChildProcessConnection extends BaseChildProcessConnection {
    private static final String TAG = "ManChildProcessConn";

    public static final Factory FACTORY = new BaseChildProcessConnection.Factory() {
        @Override
        public BaseChildProcessConnection create(Context context, DeathCallback deathCallback,
                String serviceClassName, Bundle childProcessCommonParameters,
                ChildProcessCreationParams creationParams) {
            assert LauncherThread.runningOnLauncherThread();
            return new ManagedChildProcessConnection(context, deathCallback, serviceClassName,
                    childProcessCommonParameters, creationParams);
        }
    };

    // Initial binding protects the newly spawned process from being killed before it is put to use,
    // it is maintained between calls to start() and removeInitialBinding().
    private final ChildServiceConnection mInitialBinding;

    // Strong binding will make the service priority equal to the priority of the activity. We want
    // the OS to be able to kill background renderers as it kills other background apps, so strong
    // bindings are maintained only for services that are active at the moment (between
    // addStrongBinding() and removeStrongBinding()).
    private final ChildServiceConnection mStrongBinding;

    // Low priority binding maintained in the entire lifetime of the connection, i.e. between calls
    // to start() and stop().
    private final ChildServiceConnection mWaivedBinding;

    // Incremented on addStrongBinding(), decremented on removeStrongBinding().
    private int mStrongBindingCount;

    // Moderate binding will make the service priority equal to the priority of a visible process
    // while the app is in the foreground. It will stay bound only while the app is in the
    // foreground to protect a background process from the system out-of-memory killer.
    private final ChildServiceConnection mModerateBinding;

    // Indicates whether the connection is OOM protected (if the connection is unbound, it contains
    // the state at time of unbinding).
    private boolean mOomProtected;

    // Set to true once unbind() was called.
    private boolean mUnbound;

    @VisibleForTesting
    ManagedChildProcessConnection(Context context, DeathCallback deathCallback,
            String serviceClassName, Bundle childProcessCommonParameters,
            ChildProcessCreationParams creationParams) {
        super(context, deathCallback, serviceClassName, childProcessCommonParameters,
                creationParams);

        int initialFlags = Context.BIND_AUTO_CREATE;
        int extraBindFlags = shouldBindAsExportedService() ? Context.BIND_EXTERNAL_SERVICE : 0;
        mInitialBinding = createServiceConnection(initialFlags | extraBindFlags);
        mStrongBinding = createServiceConnection(
                Context.BIND_AUTO_CREATE | Context.BIND_IMPORTANT | extraBindFlags);
        mWaivedBinding = createServiceConnection(
                Context.BIND_AUTO_CREATE | Context.BIND_WAIVE_PRIORITY | extraBindFlags);
        mModerateBinding = createServiceConnection(Context.BIND_AUTO_CREATE | extraBindFlags);
    }

    @Override
    protected boolean bind() {
        assert LauncherThread.runningOnLauncherThread();
        assert !mUnbound;
        if (!mInitialBinding.bind()) {
            return false;
        }
        updateOomProtectedState();
        mWaivedBinding.bind();
        return true;
    }

    @Override
    public void unbind() {
        assert LauncherThread.runningOnLauncherThread();
        mUnbound = true;
        mInitialBinding.unbind();
        mStrongBinding.unbind();
        // Note that we don't update the OOM state here as to preserve the last OOM state.
        mWaivedBinding.unbind();
        mModerateBinding.unbind();
        mStrongBindingCount = 0;
    }

    public boolean isInitialBindingBound() {
        assert LauncherThread.runningOnLauncherThread();
        return mInitialBinding.isBound();
    }

    public boolean isStrongBindingBound() {
        assert LauncherThread.runningOnLauncherThread();
        return mStrongBinding.isBound();
    }

    public void addInitialBinding() {
        assert LauncherThread.runningOnLauncherThread();
        mInitialBinding.bind();
        updateOomProtectedState();
    }

    public void removeInitialBinding() {
        assert LauncherThread.runningOnLauncherThread();
        mInitialBinding.unbind();
        updateOomProtectedState();
    }

    /**
     * @return true if the connection is bound and OOM protected or was OOM protected when unbound.
     */
    public boolean isOomProtectedOrWasWhenDied() {
        // WARNING: this method can be called from a thread other than the launcher thread.
        // Note that it returns the current OOM protected state and is racy. This not really
        // preventable without changing the caller's API, short of blocking.
        return mOomProtected;
    }

    public void dropOomBindings() {
        assert LauncherThread.runningOnLauncherThread();
        mInitialBinding.unbind();

        mStrongBindingCount = 0;
        mStrongBinding.unbind();
        updateOomProtectedState();

        mModerateBinding.unbind();
    }

    public void addStrongBinding() {
        assert LauncherThread.runningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        if (mStrongBindingCount == 0) {
            mStrongBinding.bind();
            updateOomProtectedState();
        }
        mStrongBindingCount++;
    }

    public void removeStrongBinding() {
        assert LauncherThread.runningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        assert mStrongBindingCount > 0;
        mStrongBindingCount--;
        if (mStrongBindingCount == 0) {
            mStrongBinding.unbind();
            updateOomProtectedState();
        }
        updateOomProtectedState();
    }

    public boolean isModerateBindingBound() {
        assert LauncherThread.runningOnLauncherThread();
        return mModerateBinding.isBound();
    }

    public void addModerateBinding() {
        assert LauncherThread.runningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        mModerateBinding.bind();
    }

    public void removeModerateBinding() {
        assert LauncherThread.runningOnLauncherThread();
        if (!isConnected()) {
            Log.w(TAG, "The connection is not bound for %d", getPid());
            return;
        }
        mModerateBinding.unbind();
    }

    // Should be called every time the mInitialBinding or mStrongBinding are bound/unbound.
    private void updateOomProtectedState() {
        if (!mUnbound) {
            mOomProtected = mInitialBinding.isBound() || mStrongBinding.isBound();
        }
    }
}
