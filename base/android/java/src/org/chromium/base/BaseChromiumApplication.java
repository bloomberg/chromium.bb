// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.os.Bundle;
import android.view.Window;

import org.chromium.base.multidex.ChromiumMultiDex;

/**
 * Basic application functionality that should be shared among all browser applications.
 */
public class BaseChromiumApplication extends Application {

    private static final String TAG = "cr.base";
    private final boolean mShouldInitializeApplicationStatusTracking;

    public BaseChromiumApplication() {
        this(true);
    }

    protected BaseChromiumApplication(boolean shouldInitializeApplicationStatusTracking) {
        mShouldInitializeApplicationStatusTracking = shouldInitializeApplicationStatusTracking;
    }

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        ChromiumMultiDex.install(this);
    }

    /**
     * Interface to be implemented by listeners for window focus events.
     */
    public interface WindowFocusChangedListener {
        /**
         * Called when the window focus changes for {@code activity}.
         * @param activity The {@link Activity} that has a window focus changed event.
         * @param hasFocus Whether or not {@code activity} gained or lost focus.
         */
        public void onWindowFocusChanged(Activity activity, boolean hasFocus);
    }

    private ObserverList<WindowFocusChangedListener> mWindowFocusListeners =
            new ObserverList<WindowFocusChangedListener>();

    @Override
    public void onCreate() {
        super.onCreate();

        if (mShouldInitializeApplicationStatusTracking) startTrackingApplicationStatus();
    }

    /**
     * Registers a listener to receive window focus updates on activities in this application.
     * @param listener Listener to receive window focus events.
     */
    public void registerWindowFocusChangedListener(WindowFocusChangedListener listener) {
        mWindowFocusListeners.addObserver(listener);
    }

    /**
     * Unregisters a listener from receiving window focus updates on activities in this application.
     * @param listener Listener that doesn't want to receive window focus events.
     */
    public void unregisterWindowFocusChangedListener(WindowFocusChangedListener listener) {
        mWindowFocusListeners.removeObserver(listener);
    }

    /** Initializes the {@link CommandLine}. */
    public void initCommandLine() {}

    /**
     * This must only be called for contexts whose application is a subclass of
     * {@link BaseChromiumApplication}.
     */
    @VisibleForTesting
    public static void initCommandLine(Context context) {
        ((BaseChromiumApplication) context.getApplicationContext()).initCommandLine();
    }

    /** Register hooks and listeners to start tracking the application status. */
    private void startTrackingApplicationStatus() {
        ApplicationStatus.initialize(this);
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(final Activity activity, Bundle savedInstanceState) {
                Window.Callback callback = activity.getWindow().getCallback();
                activity.getWindow().setCallback(new WindowCallbackWrapper(callback) {
                    @Override
                    public void onWindowFocusChanged(boolean hasFocus) {
                        super.onWindowFocusChanged(hasFocus);
                        for (WindowFocusChangedListener listener : mWindowFocusListeners) {
                            listener.onWindowFocusChanged(activity, hasFocus);
                        }
                    }
                });
            }

            @Override
            public void onActivityDestroyed(Activity activity) {
                assert activity.getWindow().getCallback() instanceof WindowCallbackWrapper;
            }

            @Override
            public void onActivityPaused(Activity activity) {
                assert activity.getWindow().getCallback() instanceof WindowCallbackWrapper;
            }

            @Override
            public void onActivityResumed(Activity activity) {
                assert activity.getWindow().getCallback() instanceof WindowCallbackWrapper;
            }

            @Override
            public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
                assert activity.getWindow().getCallback() instanceof WindowCallbackWrapper;
            }

            @Override
            public void onActivityStarted(Activity activity) {
                assert activity.getWindow().getCallback() instanceof WindowCallbackWrapper;
            }

            @Override
            public void onActivityStopped(Activity activity) {
                assert activity.getWindow().getCallback() instanceof WindowCallbackWrapper;
            }
        });
    }
}
