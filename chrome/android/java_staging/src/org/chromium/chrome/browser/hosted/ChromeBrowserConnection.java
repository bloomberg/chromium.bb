// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hosted;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.LongSparseArray;
import android.util.SparseArray;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content_public.browser.WebContents;

import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Implementation of the IBrowserConnectionService interface.
 */
class ChromeBrowserConnection extends IBrowserConnectionService.Stub {
    private static final String TAG = Log.makeTag("ChromeConnection");
    private static final long RESULT_OK = 0;
    private static final long RESULT_ERROR = -1;

    private static final Object sConstructionLock = new Object();
    private static ChromeBrowserConnection sInstance;

    private final Application mApplication;
    private final AtomicBoolean mWarmupHasBeenCalled;

    private final Object mLock;
    private final SparseArray<IBrowserConnectionCallback> mUidToCallback;
    private final LongSparseArray<Integer> mSessionIdToUid;

    private ChromeBrowserConnection(Application application) {
        super();
        mApplication = application;
        mWarmupHasBeenCalled = new AtomicBoolean();
        mLock = new Object();
        mUidToCallback = new SparseArray<IBrowserConnectionCallback>();
        mSessionIdToUid = new LongSparseArray<Integer>();
    }

    /**
     * @return The unique instance of ChromeBrowserConnection.
     */
    public static ChromeBrowserConnection getInstance(Application application) {
        synchronized (sConstructionLock) {
            if (sInstance == null) sInstance = new ChromeBrowserConnection(application);
        }
        return sInstance;
    }

    @Override
    public long finishSetup(IBrowserConnectionCallback callback) {
        final int uid = Binder.getCallingUid();
        synchronized (mLock) {
            if (mUidToCallback.get(uid) != null) return RESULT_ERROR;
            try {
                callback.asBinder().linkToDeath(new IBinder.DeathRecipient() {
                    @Override
                    public void binderDied() {
                        synchronized (mLock) {
                            cleanupAlreadyLocked(uid);
                        }
                    }
                }, 0);
            } catch (RemoteException e) {
                // The return code doesn't matter, because this executes when
                // the caller has died.
                return RESULT_ERROR;
            }
            mUidToCallback.put(uid, callback);
        }
        return RESULT_OK;
    }

    @Override
    public long warmup(long flags) {
        // Here and in mayLaunchUrl(), don't do expensive work for background applications.
        if (!isUidForeground(Binder.getCallingUid())) return RESULT_ERROR;
        if (!mWarmupHasBeenCalled.compareAndSet(false, true)) return RESULT_OK;
        // The call is non-blocking and this must execute on the UI thread, post a task.
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            @SuppressFBWarnings("DM_EXIT")
            public void run() {
                try {
                    // TODO(lizeb): Warm up more of the browser.
                    ChromiumApplication app = (ChromiumApplication) mApplication;
                    app.startBrowserProcessesAndLoadLibrariesSync(
                            app.getApplicationContext(), true);
                    ChildProcessLauncher.warmUp(app.getApplicationContext());
                } catch (ProcessInitException e) {
                    Log.e(TAG, "ProcessInitException while starting the browser process.");
                    // Cannot do anything without the native library, and cannot show a
                    // dialog to the user.
                    System.exit(-1);
                }
            }
        });
        return RESULT_OK;
    }

    @Override
    @SuppressLint("TrulyRandom") // TODO(lizeb): Figure out whether using SecureRandom is OK.
    public long newSession() {
        synchronized (mLock) {
            long sessionId;
            SecureRandom randomSource = new SecureRandom();
            do {
                sessionId = randomSource.nextLong();
                // Because Math.abs(Long.MIN_VALUE) == Long.MIN_VALUE.
                if (sessionId == Long.MIN_VALUE) continue;
                sessionId = Math.abs(sessionId);
            } while (sessionId == 0 || mSessionIdToUid.get(sessionId) != null);
            mSessionIdToUid.put(sessionId, Binder.getCallingUid());
            return sessionId;
        }
    }

    @Override
    public long mayLaunchUrl(
            long sessionId, final String url, Bundle extras, List<Bundle> otherLikelyBundles) {
        if (!isUidForeground(Binder.getCallingUid())) return RESULT_ERROR;
        synchronized (mLock) {
            if (mSessionIdToUid.get(sessionId) == null) return RESULT_ERROR;
        }
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                WarmupManager.getInstance().maybePrefetchDnsForUrlInBackground(
                        mApplication.getApplicationContext(), url);
            }
        });
        // TODO(lizeb): Prerendering.
        return sessionId;
    }

    /**
     * Transfers a prerendered WebContents if one exists.
     *
     * This resets the internal WebContents; a subsequent call to this method
     * returns null.
     *
     * @param sessionId The session ID, returned by {@link newSession}.
     * @param url The URL the WebContents is for.
     * @param extras from the intent.
     * @return The prerendered WebContents, or null.
     */
    WebContents takePrerenderedUrl(long sessionId, String url, Bundle extras) {
        // TODO(lizeb): Pre-rendering.
        return null;
    }

    /**
     * Calls the onUserNavigation callback for a given sessionId.
     *
     * This is non-blocking.
     *
     * @param sessionId Session ID associated with the callback
     * @param url URL the user has navigated to.
     * @param bundle Reserved for future use.
     * @return false if there is no client to deliver the callback to.
     */
    boolean deliverOnUserNavigationCallback(long sessionId, String url, Bundle extras) {
        synchronized (mLock) {
            if (mSessionIdToUid.get(sessionId) == null) return false;
            int uid = mSessionIdToUid.get(sessionId);
            IBrowserConnectionCallback cb = mUidToCallback.get(uid);
            if (cb == null) return false;
            try {
                cb.onUserNavigation(sessionId, url, extras);
            } catch (RemoteException e) {
                return false;
            }
        }
        return true;
    }

    /**
     * @return true iff the UID is associated with a process having a foreground importance.
     */
    private boolean isUidForeground(int uid) {
        ActivityManager am =
                (ActivityManager) mApplication.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> running = am.getRunningAppProcesses();
        for (ActivityManager.RunningAppProcessInfo rpi : running) {
            boolean isForeground =
                    rpi.importance == ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;
            if (rpi.uid == uid && isForeground) return true;
        }
        return false;
    }

    /**
     * Called when a remote client has died.
     */
    private void cleanupAlreadyLocked(int uid) {
        List<Long> keysToRemove = new ArrayList<Long>();
        // TODO(lizeb): If iterating through all the session IDs is too costly,
        // use two mappings.
        for (int i = 0; i < mSessionIdToUid.size(); i++) {
            if (mSessionIdToUid.valueAt(i) == uid) keysToRemove.add(mSessionIdToUid.keyAt(i));
        }
        for (Long sessionId : keysToRemove) {
            mSessionIdToUid.remove(sessionId);
        }
        mUidToCallback.remove(uid);
    }
}
