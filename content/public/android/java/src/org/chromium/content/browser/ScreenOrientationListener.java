// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.os.Build;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.ui.gfx.DeviceDisplayInfo;

/**
 * ScreenOrientationListener is a class that informs its observers when the
 * screen orientation changes.
 */
@VisibleForTesting
public class ScreenOrientationListener {

    /**
     * Observes changes in screen orientation.
     */
    public interface ScreenOrientationObserver {
        /**
         * Called whenever the screen orientation changes.
         *
         * @param orientation The orientation angle of the screen.
         */
        void onScreenOrientationChanged(int orientation);
    }

    /**
     * ScreenOrientationListenerBackend is an interface that abstract the
     * mechanism used for the actual screen orientation listening. The reason
     * being that from Android API Level 17 DisplayListener will be used. Before
     * that, an unreliable solution based on onConfigurationChanged has to be
     * used.
     */
    private interface ScreenOrientationListenerBackend {

        /**
         * Starts to listen for screen orientation changes. This will be called
         * when the first observer is added.
         */
        void startListening();

        /**
         * Stops to listen for screen orientation changes. This will be called
         * when the last observer is removed.
         */
        void stopListening();

        /**
         * Toggle the accurate mode if it wasn't already doing so. The backend
         * will keep track of the number of times this has been called.
         */
        void startAccurateListening();

        /**
         * Request to stop the accurate mode. It will effectively be stopped
         * only if this method is called as many times as
         * startAccurateListening().
         */
        void stopAccurateListening();
    }

    /**
     * ScreenOrientationConfigurationListener implements ScreenOrientationListenerBackend
     * to use ComponentCallbacks in order to listen for screen orientation
     * changes.
     *
     * This method is known to not correctly detect 180 degrees changes but it
     * is the only method that will work before API Level 17 (excluding polling).
     * When toggleAccurateMode() is called, it will start polling in order to
     * find out if the display has changed.
     */
    private class ScreenOrientationConfigurationListener
            implements ScreenOrientationListenerBackend, ComponentCallbacks {

        private static final long POLLING_DELAY = 500;

        private int mAccurateCount = 0;

        // ScreenOrientationListenerBackend implementation:

        @Override
        public void startListening() {
            mAppContext.registerComponentCallbacks(this);
        }

        @Override
        public void stopListening() {
            mAppContext.unregisterComponentCallbacks(this);
        }

        @Override
        public void startAccurateListening() {
            ++mAccurateCount;

            if (mAccurateCount > 1)
                return;

            // Start polling if we went from 0 to 1. The polling will
            // automatically stop when mAccurateCount reaches 0.
            final ScreenOrientationConfigurationListener self = this;
            ThreadUtils.postOnUiThreadDelayed(new Runnable() {
                @Override
                public void run() {
                    self.onConfigurationChanged(null);

                    if (self.mAccurateCount < 1)
                        return;

                    ThreadUtils.postOnUiThreadDelayed(this,
                            ScreenOrientationConfigurationListener.POLLING_DELAY);
                }
            }, POLLING_DELAY);
        }

        @Override
        public void stopAccurateListening() {
            --mAccurateCount;
            assert mAccurateCount >= 0;
        }

        // ComponentCallbacks implementation:

        @Override
        public void onConfigurationChanged(Configuration newConfig) {
            notifyObservers();
        }

        @Override
        public void onLowMemory() {
        }
    }

    /**
     * ScreenOrientationDisplayListener implements ScreenOrientationListenerBackend
     * to use DisplayListener in order to listen for screen orientation changes.
     *
     * This method is reliable but DisplayListener is only available for API Level 17+.
     */
    @SuppressLint("NewApi")
    private class ScreenOrientationDisplayListener
            implements ScreenOrientationListenerBackend, DisplayListener {

        // ScreenOrientationListenerBackend implementation:

        @Override
        public void startListening() {
            DisplayManager displayManager =
                    (DisplayManager) mAppContext.getSystemService(Context.DISPLAY_SERVICE);
            displayManager.registerDisplayListener(this, null);
        }

        @Override
        public void stopListening() {
            DisplayManager displayManager =
                    (DisplayManager) mAppContext.getSystemService(Context.DISPLAY_SERVICE);
            displayManager.unregisterDisplayListener(this);
        }

        @Override
        public void startAccurateListening() {
            // Always accurate. Do nothing.
        }

        @Override
        public void stopAccurateListening() {
            // Always accurate. Do nothing.
        }

        // DisplayListener implementation:

        @Override
        public void onDisplayAdded(int displayId) {
        }

        @Override
        public void onDisplayRemoved(int displayId) {
        }

        @Override
        public void onDisplayChanged(int displayId) {
            notifyObservers();
        }

    }

    private static final String TAG = "ScreenOrientationListener";

    // List of observers to notify when the screen orientation changes.
    private final ObserverList<ScreenOrientationObserver> mObservers =
            new ObserverList<ScreenOrientationObserver>();

    // mOrientation will be updated every time the orientation changes. When not
    // listening for changes, the value will be invalid and will be updated when
    // starting to listen again.
    private int mOrientation;

    // Current application context derived from the first context being received.
    private Context mAppContext;

    private ScreenOrientationListenerBackend mBackend;

    private static ScreenOrientationListener sInstance;

    /**
     * Returns a ScreenOrientationListener implementation based on the device's
     * supported API level.
     */
    public static ScreenOrientationListener getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) {
            sInstance = new ScreenOrientationListener();
        }

        return sInstance;
    }

    private ScreenOrientationListener() {
        mBackend = Build.VERSION.SDK_INT >= 17 ?
                new ScreenOrientationDisplayListener() :
                new ScreenOrientationConfigurationListener();
    }

    /**
     * Add |observer| in the ScreenOrientationListener observer list and
     * immediately call |onScreenOrientationChanged| on it with the current
     * orientation value.
     *
     * @param observer The observer that will get notified.
     * @param context The context associated with this observer.
     */
    public void addObserver(ScreenOrientationObserver observer, Context context) {
        if (mAppContext == null) {
            mAppContext = context.getApplicationContext();
        }

        assert mAppContext == context.getApplicationContext();
        assert mAppContext != null;

        if (!mObservers.addObserver(observer)) {
            Log.w(TAG, "Adding an observer that is already present!");
            return;
        }

        // If we got our first observer, we should start listening.
        if (mObservers.size() == 1) {
            updateOrientation();
            mBackend.startListening();
        }

        // We need to send the current value to the added observer as soon as
        // possible but outside of the current stack.
        final ScreenOrientationObserver obs = observer;
        ThreadUtils.assertOnUiThread();
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                obs.onScreenOrientationChanged(mOrientation);
            }
        });
    }

    /**
     * Remove the |observer| from the ScreenOrientationListener observer list.
     *
     * @param observer The observer that will no longer receive notification.
     */
    public void removeObserver(ScreenOrientationObserver observer) {
        if (!mObservers.removeObserver(observer)) {
            Log.w(TAG, "Removing an inexistent observer!");
            return;
        }

        if (mObservers.isEmpty()) {
            // The last observer was removed, we should just stop listening.
            mBackend.stopListening();
        }
    }

    /**
     * Toggle the accurate mode if it wasn't already doing so. The backend will
     * keep track of the number of times this has been called.
     */
    public void startAccurateListening() {
        mBackend.startAccurateListening();
    }

    /**
     * Request to stop the accurate mode. It will effectively be stopped only if
     * this method is called as many times as startAccurateListening().
     */
    public void stopAccurateListening() {
        mBackend.stopAccurateListening();
    }

    /**
     * This should be called by classes extending ScreenOrientationListener when
     * it is possible that there is a screen orientation change. If there is an
     * actual change, the observers will get notified.
     */
    private void notifyObservers() {
        int previousOrientation = mOrientation;
        updateOrientation();

        if (mOrientation == previousOrientation) {
            return;
        }

        DeviceDisplayInfo.create(mAppContext).updateNativeSharedDisplayInfo();

        for (ScreenOrientationObserver observer : mObservers) {
            observer.onScreenOrientationChanged(mOrientation);
        }
    }

    /**
     * Updates |mOrientation| based on the default display rotation.
     */
    private void updateOrientation() {
        WindowManager windowManager =
                (WindowManager) mAppContext.getSystemService(Context.WINDOW_SERVICE);

        switch (windowManager.getDefaultDisplay().getRotation()) {
            case Surface.ROTATION_0:
                mOrientation = 0;
                break;
            case Surface.ROTATION_90:
                mOrientation = 90;
                break;
            case Surface.ROTATION_180:
                mOrientation = 180;
                break;
            case Surface.ROTATION_270:
                mOrientation = -90;
                break;
            default:
                throw new IllegalStateException(
                        "Display.getRotation() shouldn't return that value");
        }
    }
}
