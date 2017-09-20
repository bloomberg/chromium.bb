// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.download;

import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.HashSet;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * A class that handles logic related to observers that are waiting to see when the
 * DownloadsForegroundService is shutting down or starting back up.
 */
public final class DownloadForegroundServiceObservers {
    private static final String TAG = "DownloadFgServiceObs";
    private static final String KEY_FOREGROUND_SERVICE_OBSERVERS = "ForegroundServiceObservers";

    /**
     * An Observer interfaces that allows other classes to know when this service is shutting down.
     * There is a requirement for any implementations to have a public, no-param constructor.
     */
    public interface Observer {
        /**
         * Called when the foreground service was automatically restarted because of START_STICKY.
         */
        void onForegroundServiceRestarted();

        /**
         * Called when any task (service or activity) is removed from the service's application.
         */
        void onForegroundServiceTaskRemoved();

        /**
         * Called when the foreground service is destroyed.
         */
        void onForegroundServiceDestroyed();
    }

    /**
     * Adds an observer, which will be notified when this service attempts to start stopping itself.
     * @param observerClass to be added to the list of observers in SharedPrefs.
     */
    public static void addObserver(Class<? extends Observer> observerClass) {
        ThreadUtils.assertOnUiThread();

        Set<String> observers = getAllObservers();
        String observerClassName = observerClass.getName();
        if (observers.contains(observerClassName)) return;

        // Set returned from getStringSet() should not be modified.
        observers = new HashSet<>(observers);
        observers.add(observerClassName);

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().putStringSet(KEY_FOREGROUND_SERVICE_OBSERVERS, observers).apply();
    }

    /**
     * Removes observer, which will no longer be notified when this class starts stopping itself.
     * @param observerClass to be removed from the list of observers in SharedPrefs.
     */
    public static void removeObserver(Class<? extends Observer> observerClass) {
        ThreadUtils.assertOnUiThread();

        Set<String> observers = getAllObservers();
        String observerClassName = observerClass.getName();
        if (!observers.contains(observerClassName)) return;

        // Set returned from getStringSet() should not be modified.
        observers = new HashSet<>(observers);
        observers.remove(observerClassName);

        // Clear observer list if there are none.
        if (observers.size() == 0) {
            removeAllObservers();
            return;
        }

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().putStringSet(KEY_FOREGROUND_SERVICE_OBSERVERS, observers).apply();
    }

    static void alertObserversServiceRestarted() {
        Set<String> observers = getAllObservers();
        removeAllObservers();

        for (String observerClassName : observers) {
            DownloadForegroundServiceObservers.Observer observer =
                    DownloadForegroundServiceObservers.getObserverFromClassName(observerClassName);
            if (observer != null) observer.onForegroundServiceRestarted();
        }
    }

    static void alertObserversServiceDestroyed() {
        Set<String> observers = getAllObservers();

        for (String observerClassName : observers) {
            DownloadForegroundServiceObservers.Observer observer =
                    DownloadForegroundServiceObservers.getObserverFromClassName(observerClassName);
            if (observer != null) observer.onForegroundServiceDestroyed();
        }
    }

    static void alertObserversTaskRemoved() {
        Set<String> observers = getAllObservers();

        for (String observerClassName : observers) {
            Observer observer = getObserverFromClassName(observerClassName);
            if (observer != null) observer.onForegroundServiceTaskRemoved();
        }
    }

    private static Set<String> getAllObservers() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        return prefs.getStringSet(KEY_FOREGROUND_SERVICE_OBSERVERS, new HashSet<String>(1));
    }

    private static void removeAllObservers() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(KEY_FOREGROUND_SERVICE_OBSERVERS)
                .apply();
    }

    @Nullable
    private static Observer getObserverFromClassName(String observerClassName) {
        if (observerClassName == null) return null;

        Class<?> observerClass;
        try {
            observerClass = Class.forName(observerClassName);
        } catch (ClassNotFoundException e) {
            Log.w(TAG, "Unable to find observer class with name " + observerClassName);
            return null;
        }

        if (!Observer.class.isAssignableFrom(observerClass)) {
            Log.w(TAG, "Class " + observerClass + " is not an observer");
            return null;
        }

        try {
            return (Observer) observerClass.newInstance();
        } catch (InstantiationException e) {
            Log.w(TAG, "Unable to instantiate class (InstExc) " + observerClass);
            return null;
        } catch (IllegalAccessException e) {
            Log.w(TAG, "Unable to instantiate class (IllAccExc) " + observerClass);
            return null;
        }
    }
}
