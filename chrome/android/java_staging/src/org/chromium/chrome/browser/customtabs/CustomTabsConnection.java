// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.Application;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Point;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.LongSparseArray;
import android.util.SparseArray;
import android.view.WindowManager;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content_public.browser.WebContents;

import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Implementation of the ICustomTabsConnectionService interface.
 */
class CustomTabsConnection extends ICustomTabsConnectionService.Stub {
    private static final String TAG = "cr.ChromeConnection";
    private static final long RESULT_OK = 0;
    private static final long RESULT_ERROR = -1;
    private static final String KEY_CUSTOM_TABS_REFERRER = "android.support.CUSTOM_TABS:referrer";

    // Values for the "CustomTabs.PredictionStatus" UMA histogram. Append-only.
    private static final int NO_PREDICTION = 0;
    private static final int GOOD_PREDICTION = 1;
    private static final int BAD_PREDICTION = 2;
    private static final int PREDICTION_STATUS_COUNT = 3;

    private static final Object sConstructionLock = new Object();
    private static CustomTabsConnection sInstance;

    private static final class PrerenderedUrlParams {
        public final long mSessionId;
        public final WebContents mWebContents;
        public final String mUrl;
        public final Bundle mExtras;

        PrerenderedUrlParams(long sessionId, WebContents webContents, String url, Bundle extras) {
            mSessionId = sessionId;
            mWebContents = webContents;
            mUrl = url;
            mExtras = extras;
        }
    }

    private final Application mApplication;
    private final AtomicBoolean mWarmupHasBeenCalled;
    private ExternalPrerenderHandler mExternalPrerenderHandler;
    private PrerenderedUrlParams mPrerender;

    /** Per-sessionId values. */
    private static class SessionParams {
        public final int mUid;
        private ServiceConnection mServiceConnection;
        private String mPredictedUrl;
        private long mLastMayLaunchUrlTimestamp;

        public SessionParams(int uid) {
            mUid = uid;
            mServiceConnection = null;
            mPredictedUrl = null;
            mLastMayLaunchUrlTimestamp = 0;
        }

        public ServiceConnection getServiceConnection() {
            return mServiceConnection;
        }

        public void setServiceConnection(ServiceConnection serviceConnection) {
            mServiceConnection = serviceConnection;
        }

        public void setPredictionMetrics(String predictedUrl, long lastMayLaunchUrlTimestamp) {
            mPredictedUrl = predictedUrl;
            mLastMayLaunchUrlTimestamp = lastMayLaunchUrlTimestamp;
        }

        public String getPredictedUrl() {
            return mPredictedUrl;
        }

        public long getLastMayLaunchUrlTimestamp() {
            return mLastMayLaunchUrlTimestamp;
        }
    }

    private final Object mLock;
    private final SparseArray<ICustomTabsConnectionCallback> mUidToCallback;
    private final LongSparseArray<SessionParams> mSessionParams;

    private CustomTabsConnection(Application application) {
        super();
        mApplication = application;
        mWarmupHasBeenCalled = new AtomicBoolean();
        mLock = new Object();
        mUidToCallback = new SparseArray<ICustomTabsConnectionCallback>();
        mSessionParams = new LongSparseArray<SessionParams>();
    }

    /**
     * @return The unique instance of ChromeCustomTabsConnection.
     */
    public static CustomTabsConnection getInstance(Application application) {
        synchronized (sConstructionLock) {
            if (sInstance == null) sInstance = new CustomTabsConnection(application);
        }
        return sInstance;
    }

    @Override
    public long finishSetup(ICustomTabsConnectionCallback callback) {
        if (callback == null) return RESULT_ERROR;
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
        if (!isUidForegroundOrSelf(Binder.getCallingUid())) return RESULT_ERROR;
        if (!mWarmupHasBeenCalled.compareAndSet(false, true)) return RESULT_OK;
        // The call is non-blocking and this must execute on the UI thread, post a task.
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            @SuppressFBWarnings("DM_EXIT")
            public void run() {
                try {
                    // TODO(lizeb): Warm up more of the browser.
                    ChromiumApplication app = (ChromiumApplication) mApplication;
                    app.startBrowserProcessesAndLoadLibrariesSync(true);
                    final Context context = app.getApplicationContext();
                    new AsyncTask<Void, Void, Void>() {
                        @Override
                        protected Void doInBackground(Void... params) {
                            ChildProcessLauncher.warmUp(context);
                            return null;
                        }
                    }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
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
            } while (sessionId == 0 || mSessionParams.get(sessionId) != null);
            mSessionParams.put(sessionId, new SessionParams(Binder.getCallingUid()));
            return sessionId;
        }
    }

    @Override
    public long mayLaunchUrl(final long sessionId, final String url, final Bundle extras,
            List<Bundle> otherLikelyBundles) {
        int uid = Binder.getCallingUid();
        // Don't do anything for unknown schemes. Not having a scheme is
        // allowed, as we allow "www.example.com".
        String scheme = Uri.parse(url).normalizeScheme().getScheme();
        if (scheme != null && !scheme.equals("http") && !scheme.equals("https")) {
            return RESULT_ERROR;
        }
        if (!isUidForegroundOrSelf(uid)) return RESULT_ERROR;
        synchronized (mLock) {
            SessionParams sessionParams = mSessionParams.get(sessionId);
            if (sessionParams == null || sessionParams.mUid != uid) return RESULT_ERROR;
            sessionParams.setPredictionMetrics(url, SystemClock.elapsedRealtime());
        }
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (!TextUtils.isEmpty(url)) {
                    WarmupManager.getInstance().maybePrefetchDnsForUrlInBackground(
                            mApplication.getApplicationContext(), url);
                }
                // Calling with a null or empty url cancels a current prerender.
                prerenderUrl(sessionId, url, extras);
            }
        });
        return sessionId;
    }

    /**
     * Registers a launch of a |url| for a given |sessionId|.
     *
     * This is used for accounting.
     */
    void registerLaunch(long sessionId, String url) {
        int outcome;
        long elapsedTimeMs = -1;
        synchronized (mLock) {
            SessionParams sessionParams = mSessionParams.get(sessionId);
            if (sessionParams == null) {
                outcome = NO_PREDICTION;
            } else {
                String predictedUrl = sessionParams.getPredictedUrl();
                outcome = predictedUrl == null ? NO_PREDICTION
                        : predictedUrl.equals(url) ? GOOD_PREDICTION : BAD_PREDICTION;
                elapsedTimeMs = SystemClock.elapsedRealtime()
                        - sessionParams.getLastMayLaunchUrlTimestamp();
                sessionParams.setPredictionMetrics(null, 0);
            }
        }
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.PredictionStatus", outcome, PREDICTION_STATUS_COUNT);
        if (outcome == GOOD_PREDICTION) {
            RecordHistogram.recordCustomTimesHistogram("CustomTabs.PredictionToLaunch",
                    elapsedTimeMs, 1, TimeUnit.MINUTES.toMillis(3), TimeUnit.MILLISECONDS, 100);
        }
    }

    /**
     * Transfers a prerendered WebContents if one exists.
     *
     * This resets the internal WebContents; a subsequent call to this method
     * returns null. Must be called from the UI thread.
     * If a prerender exists for a different URL with the same sessionId, then
     * this is treated as a mispredict from the client application, and cancels
     * the previous prerender. This is done to avoid keeping resources laying
     * around for too long, but is subject to a race condition, as the following
     * scenario is possible:
     * The application calls:
     * 1. mayLaunchUrl(url1) <- IPC
     * 2. loadUrl(url2) <- Intent
     * 3. mayLaunchUrl(url3) <- IPC
     * If the IPC for url3 arrives before the intent for url2, then this methods
     * cancels the prerender for url3, which is unexpected. On the other
     * hand, not cancelling the previous prerender leads to wasted resources, as
     * a WebContents is lingering. This can be solved by requiring applications
     * to call mayLaunchUrl(null) to cancel a current prerender before 2, that
     * is for a mispredict.
     *
     * @param sessionId The session ID, returned by {@link newSession}.
     * @param url The URL the WebContents is for.
     * @param extras from the intent.
     * @return The prerendered WebContents, or null.
     */
    WebContents takePrerenderedUrl(long sessionId, String url, Bundle extras) {
        ThreadUtils.assertOnUiThread();
        if (mPrerender == null || mPrerender.mSessionId != sessionId) return null;
        WebContents webContents = mPrerender.mWebContents;
        String prerenderedUrl = mPrerender.mUrl;
        mPrerender = null;
        // TODO(lizeb): Referrer
        if (TextUtils.equals(prerenderedUrl, url)) return webContents;
        mExternalPrerenderHandler.cancelCurrentPrerender();
        webContents.destroy();
        return null;
    }

    private ICustomTabsConnectionCallback getCallbackForSessionIdAlreadyLocked(long sessionId) {
        SessionParams sessionParams = mSessionParams.get(sessionId);
        if (sessionParams == null) return null;
        return mUidToCallback.get(sessionParams.mUid);
    }

    /**
     * Notifies the application that a page load has started.
     *
     * Delivers the {@link ICustomTabsConnectionCallback#onUserNavigationStarted}
     * callback to the aplication.
     *
     * @param sessionId The session ID.
     * @param url The URL the tab is navigating to.
     * @return true for success.
     */
    boolean notifyPageLoadStarted(long sessionId, String url) {
        synchronized (mLock) {
            ICustomTabsConnectionCallback cb = getCallbackForSessionIdAlreadyLocked(sessionId);
            if (cb == null) return false;
            try {
                cb.onUserNavigationStarted(sessionId, url, null);
            } catch (RemoteException e) {
                return false;
            }
        }
        return true;
    }

    /**
     * Notifies the application that a page load has finished.
     *
     * Delivers the {@link ICustomTabsConnectionCallback#onUserNavigationFinished}
     * callback to the aplication.
     *
     * @param sessionId The session ID.
     * @param url The URL the tab has navigated to.
     * @return true for success.
     */
    boolean notifyPageLoadFinished(long sessionId, String url) {
        synchronized (mLock) {
            ICustomTabsConnectionCallback cb = getCallbackForSessionIdAlreadyLocked(sessionId);
            if (cb == null) return false;
            try {
                cb.onUserNavigationFinished(sessionId, url, null);
            } catch (RemoteException e) {
                return false;
            }
        }
        return true;
    }

    /**
     * Keeps the application linked with sessionId alive.
     *
     * The application is kept alive (that is, raised to at least the current
     * process priority level) until {@link dontKeepAliveForSessionId()} is
     * called.
     *
     * @param sessionId Session ID provided in the intent.
     * @param intent Intent describing the service to bind to.
     * @return true for success.
     */
    boolean keepAliveForSessionId(long sessionId, Intent intent) {
        // When an application is bound to a service, its priority is raised to
        // be at least equal to the application's one. This binds to a dummy
        // service (no calls to this service are made).
        if (intent == null || intent.getComponent() == null) return false;
        SessionParams sessionParams;
        synchronized (mLock) {
            sessionParams = mSessionParams.get(sessionId);
            if (sessionParams == null) return false;
        }
        String packageName = intent.getComponent().getPackageName();
        PackageManager pm = mApplication.getApplicationContext().getPackageManager();
        // Only binds to the application associated to this session ID.
        int uid = sessionParams.mUid;
        if (!Arrays.asList(pm.getPackagesForUid(uid)).contains(packageName)) return false;
        Intent serviceIntent = new Intent().setComponent(intent.getComponent());
        // This ServiceConnection doesn't handle disconnects. This is on
        // purpose, as it occurs when the remote process has died. Since the
        // only use of this connection is to keep the application alive,
        // re-connecting would just re-create the process, but the application
        // state has been lost at that point, the callbacks invalidated, etc.
        ServiceConnection connection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {}
            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };
        boolean ok;
        try {
            ok = mApplication.getApplicationContext().bindService(
                    serviceIntent, connection, Context.BIND_AUTO_CREATE);
        } catch (SecurityException e) {
            return false;
        }
        if (ok) sessionParams.setServiceConnection(connection);
        return ok;
    }

    /**
     * Lets the lifetime of the process linked to a given sessionId be managed normally.
     *
     * Without a matching call to {@link keepAliveForSessionId}, this is a no-op.
     *
     * @param sessionId Session ID, as provided to {@link keepAliveForSessionId}.
     */
    void dontKeepAliveForSessionId(long sessionId) {
        SessionParams sessionParams;
        synchronized (mLock) {
            sessionParams = mSessionParams.get(sessionId);
        }
        if (sessionParams == null || sessionParams.getServiceConnection() == null) return;
        ServiceConnection serviceConnection = sessionParams.getServiceConnection();
        sessionParams.setServiceConnection(null);
        mApplication.getApplicationContext().unbindService(serviceConnection);
    }

    /**
     * @return true if the |uid| is associated with a process having a
     * foreground importance, or self.
     */
    private boolean isUidForegroundOrSelf(int uid) {
        if (uid == Process.myUid()) return true;
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
     * {@link cleanupAlreadyLocked}, without holding mLock.
     */
    @VisibleForTesting
    void cleanup(int uid) {
        synchronized (mLock) {
            cleanupAlreadyLocked(uid);
        }
    }

    /**
     * Called when a remote client has died.
     */
    private void cleanupAlreadyLocked(int uid) {
        List<Long> keysToRemove = new ArrayList<Long>();
        // TODO(lizeb): If iterating through all the session IDs is too costly,
        // use two mappings.
        for (int i = 0; i < mSessionParams.size(); i++) {
            if (mSessionParams.valueAt(i).mUid == uid) keysToRemove.add(mSessionParams.keyAt(i));
        }
        for (Long sessionId : keysToRemove) {
            mSessionParams.remove(sessionId);
        }
        mUidToCallback.remove(uid);
    }

    private boolean mayPrerender() {
        ConnectivityManager cm =
                (ConnectivityManager) mApplication.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        return !cm.isActiveNetworkMetered();
    }

    private void prerenderUrl(long sessionId, String url, Bundle extras) {
        ThreadUtils.assertOnUiThread();
        // TODO(lizeb): Prerendering through ChromePrerenderService is
        // incompatible with prerendering through this service. Remove this
        // limitation, or remove ChromePrerenderService.
        WarmupManager.getInstance().disallowPrerendering();

        if (!mayPrerender()) return;
        if (!mWarmupHasBeenCalled.get()) return;
        // Last one wins and cancels the previous prerender.
        if (mPrerender != null) {
            mExternalPrerenderHandler.cancelCurrentPrerender();
            mPrerender.mWebContents.destroy();
            mPrerender = null;
        }
        if (TextUtils.isEmpty(url)) return;
        if (mExternalPrerenderHandler == null) {
            mExternalPrerenderHandler = new ExternalPrerenderHandler();
        }
        Point contentSize = estimateContentSize();
        String referrer = "";
        if (extras != null) referrer = extras.getString(KEY_CUSTOM_TABS_REFERRER, "");
        WebContents webContents = mExternalPrerenderHandler.addPrerender(
                Profile.getLastUsedProfile(), url, referrer, contentSize.x, contentSize.y);
        mPrerender = new PrerenderedUrlParams(sessionId, webContents, url, extras);
    }

    /**
     * Provides an estimate of the contents size.
     *
     * The estimate is likely to be incorrect. This is not a problem, as the aim
     * is to avoid getting a different layout and resources than needed at
     * render time.
     */
    private Point estimateContentSize() {
        // The size is estimated as:
        // X = screenSizeX
        // Y = screenSizeY - top bar - bottom bar - custom tabs bar
        Point screenSize = new Point();
        WindowManager wm = (WindowManager) mApplication.getSystemService(Context.WINDOW_SERVICE);
        wm.getDefaultDisplay().getSize(screenSize);
        Resources resources = mApplication.getResources();
        int statusBarId = resources.getIdentifier("status_bar_height", "dimen", "android");
        int navigationBarId = resources.getIdentifier("navigation_bar_height", "dimen", "android");
        try {
            screenSize.y -=
                    resources.getDimensionPixelSize(R.dimen.custom_tabs_control_container_height);
            screenSize.y -= resources.getDimensionPixelSize(statusBarId);
            screenSize.y -= resources.getDimensionPixelSize(navigationBarId);
        } catch (Resources.NotFoundException e) {
            // Nothing, this is just a best effort estimate.
        }
        return screenSize;
    }
}
