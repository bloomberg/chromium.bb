// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.support.v7.app.ActionBarActivity;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.metrics.MemoryUma;
import org.chromium.chrome.browser.profiles.Profile;


/**
 * An activity that talks with application and activity level delegates for async initialization.
 */
public abstract class AsyncInitializationActivity extends ActionBarActivity implements
        ChromeActivityNativeDelegate, BrowserParts {
    protected final Handler mHandler;

    // Time at which onCreate is called. This is realtime, counted in ms since device boot.
    private long mOnCreateTimestampMs;

    // Time at which onCreate is called. This is uptime, to be sent to native code.
    private long mOnCreateTimestampUptimeMs;
    private Bundle mSavedInstanceState;
    private boolean mDestroyed;
    private NativeInitializationController mNativeInitializationController;
    private MemoryUma mMemoryUma;

    public AsyncInitializationActivity() {
        mHandler = new Handler();
    }

    @Override
    protected void onDestroy() {
        mDestroyed = true;
        super.onDestroy();
    }

    @Override
    public void preInflationStartup() { }

    @Override
    public final void setContentViewAndLoadLibrary() {
        // setContentView inflating the decorView and the basic UI hierarhcy as stubs.
        // This is done here before kicking long running I/O because inflation includes accessing
        // resource files(xmls etc) even if we are inflating views defined by the framework. If this
        // operation gets blocked because other long running I/O are running, we delay onCreate(),
        // onStart() and first draw consequently.

        setContentView();
        if (mLaunchBehindWorkaround != null) mLaunchBehindWorkaround.onSetContentView();

        // Kick off long running IO tasks that can be done in parallel.
        mNativeInitializationController = new NativeInitializationController(this, this);
        mNativeInitializationController.startBackgroundTasks();
    }

    @Override
    public void postInflationStartup() { }

    @Override
    public void maybePreconnect() {
        TraceEvent.begin("maybePreconnect");
        Intent intent = getIntent();
        if (intent != null && intent.ACTION_VIEW.equals(intent.getAction())) {
            final String url = intent.getDataString();
            WarmupManager.getInstance()
                .maybePreconnectUrlAndSubResources(Profile.getLastUsedProfile(), url);
        }
        TraceEvent.end("maybePreconnect");
    }

    @Override
    public void initializeCompositor() { }

    @Override
    public void initializeState() { }

    @Override
    public void finishNativeInitialization() {
        mMemoryUma = new MemoryUma();
        mNativeInitializationController.onNativeInitializationComplete();
    }

    @Override
    public void onStartupFailure() {
        finish();
    }

    /**
     * Extending classes should override {@link AsyncInitializationActivity#preInflationStartup()},
     * {@link AsyncInitializationActivity#setContentView()} and
     * {@link AsyncInitializationActivity#postInflationStartup()} instead of this call which will
     * be called on that order.
     */
    @Override
    protected final void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mOnCreateTimestampMs = SystemClock.elapsedRealtime();
        mOnCreateTimestampUptimeMs = SystemClock.uptimeMillis();
        mSavedInstanceState = savedInstanceState;

        ChromeBrowserInitializer.getInstance(this).handlePreNativeStartup(this);
    }

    /**
     * @return The elapsed real time for the activity creation in ms.
     */
    protected long getOnCreateTimestampUptimeMs() {
        return mOnCreateTimestampUptimeMs;
    }

    /**
     * @return The uptime for the activity creation in ms.
     */
    protected long getOnCreateTimestampMs() {
        return mOnCreateTimestampMs;
    }

    /**
     * @return The saved bundle for the last recorded state.
     */
    protected Bundle getSavedInstanceState() {
        return mSavedInstanceState;
    }

    /**
     * Resets the saved state and makes it unavailable for the rest of the activity lifecycle.
     */
    protected void resetSavedInstanceState() {
        mSavedInstanceState = null;
    }

    @Override
    public void onStart() {
        super.onStart();
        mNativeInitializationController.onStart();
    }

    @Override
    public void onResume() {
        super.onResume();
        mNativeInitializationController.onResume();
        if (mLaunchBehindWorkaround != null) mLaunchBehindWorkaround.onResume();
    }

    @Override
    public void onPause() {
        mNativeInitializationController.onPause();
        super.onPause();
        if (mLaunchBehindWorkaround != null) mLaunchBehindWorkaround.onPause();
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mMemoryUma != null) mMemoryUma.onStop();
        mNativeInitializationController.onStop();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (intent == null) return;
        mNativeInitializationController.onNewIntent(intent);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        mNativeInitializationController.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public final void onCreateWithNative() {
        ChromeBrowserInitializer.getInstance(this).handlePostNativeStartup(this);
    }

    @Override
    public void onStartWithNative() { }

    @Override
    public boolean isActivityDestroyed() {
        return mDestroyed;
    }

    @Override
    public final void onFirstDrawComplete() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mNativeInitializationController.firstDrawComplete();
            }
        });
    }

    @Override
    public boolean hasDoneFirstDraw() {
        return true;
    }

    @Override
    public void onNewIntentWithNative(Intent intent) { }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent data) {
        return false;
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        if (mMemoryUma != null) mMemoryUma.onLowMemory();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (mMemoryUma != null) mMemoryUma.onTrimMemory(level);
    }


    /**
     * Extending classes should implement this and call {@link Activity#setContentView(int)} in it.
     */
    protected abstract void setContentView();

    /**
     * Lollipop (pre-MR1) makeTaskLaunchBehind() workaround.
     *
     * Our activity's surface is destroyed at the end of the new activity animation
     * when ActivityOptions.makeTaskLaunchBehind() is used, which causes a crash.
     * Making everything invisible when paused prevents the crash, since view changes
     * will not trigger draws to the missing surface. However, we need to wait until
     * after the first draw to make everything invisible, as the activity launch
     * animation needs a full frame (or it will delay the animation excessively).
     */
    private final LaunchBehindWorkaround mLaunchBehindWorkaround =
            (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP)
                    ? new LaunchBehindWorkaround()
                    : null;

    private class LaunchBehindWorkaround {
        private boolean mPaused = false;

        private View getDecorView() {
            return getWindow().getDecorView();
        }

        private ViewTreeObserver getViewTreeObserver() {
            return getDecorView().getViewTreeObserver();
        }

        private void onPause() {
            mPaused = true;
        }

        public void onResume() {
            mPaused = false;
            getDecorView().setVisibility(View.VISIBLE);
        }

        public void onSetContentView() {
            getViewTreeObserver().addOnPreDrawListener(mPreDrawListener);
        }

        // Note, we probably want onDrawListener here, but it isn't being called
        // when I add this to the decorView. However, it should be the same for
        // this purpose as long as no other pre-draw listener returns false.
        private final OnPreDrawListener mPreDrawListener = new OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mPaused) {
                            getDecorView().setVisibility(View.GONE);
                        }
                        getViewTreeObserver().removeOnPreDrawListener(mPreDrawListener);
                    }
                });
                return true;
            }
        };
    }
}
