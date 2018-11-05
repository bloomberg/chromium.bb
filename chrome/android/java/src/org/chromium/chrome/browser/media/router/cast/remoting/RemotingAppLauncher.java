// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast.remoting;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;

import com.google.android.gms.common.api.GoogleApiClient;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.util.ArrayList;
import java.util.List;

/**
 * Class that manages the instantiation of RemotingApps.
 *
 * A RemotingApp is essentially a wrapper around an application ID. We use them to protect certain
 * application IDs in downstream code, and to override some app specific logic. The IDs correspond
 * to Cast receiver apps.
 *
 * The types of RemotingApps are discovered by reflection. We find the class names via the
 * REMOTE_PLAYBACK_APPS_KEY in AndroidManifest.xml. This allows us to specify different apps in the
 * default chromium manifest, versus the official Chrome on Android manifest.
 */
public class RemotingAppLauncher {
    private static final String TAG = "RemoteAppLnchr";

    // This is a key for meta-data in the package manifest.
    private static final String REMOTE_PLAYBACK_APPS_KEY =
            "org.chromium.content.browser.REMOTE_PLAYBACK_APPS";

    private static RemotingAppLauncher sInstance;

    /**
     * Interface that represents a Cast receiver app, that is compatible with remote playback.
     */
    public interface RemotingApp {
        /**
         * Returns true if the given app supports the source
         */
        public boolean canPlayMedia(RemotingMediaSource source);

        /**
         * Returns the application ID of the receiver app.
         * NOTE: This string should not be added to logs, since this can be overriden
         * by downstream and we would leak internal app IDs.
         */
        public String getApplicationId();

        /**
         * Returns true if RemoteMediaPlayer.load() should be avoided, and RemotingApp.load() should
         * be called instead.
         */
        public boolean hasCustomLoad();

        /**
         * Loads the given source, at the given start time, for the already connected client.
         */
        public void load(GoogleApiClient client, long startTime, MediaSource source);
    }

    private List<RemotingApp> mRemotingApps = new ArrayList<RemotingApp>();

    /**
     * The private constructor to make sure the object is only created by the instance() method.
     */
    private RemotingAppLauncher() {}

    private void createRemotingApps() {
        // We only need to do this once
        if (!mRemotingApps.isEmpty()) return;
        try {
            Activity currentActivity = ApplicationStatus.getLastTrackedFocusedActivity();
            ApplicationInfo ai = currentActivity.getPackageManager().getApplicationInfo(
                    currentActivity.getPackageName(), PackageManager.GET_META_DATA);
            Bundle bundle = ai.metaData;
            String classNameString = bundle.getString(REMOTE_PLAYBACK_APPS_KEY);

            if (classNameString != null) {
                String[] classNames = classNameString.split(",");
                for (String className : classNames) {
                    Log.d(TAG, "Adding remoting app %s", className.trim());
                    Class<?> remotingAppClass = Class.forName(className.trim());
                    Object remotingApp = remotingAppClass.newInstance();
                    mRemotingApps.add((RemotingApp) remotingApp);
                }
            }
        } catch (NameNotFoundException | ClassNotFoundException | SecurityException
                | InstantiationException | IllegalAccessException | IllegalArgumentException e) {
            // Should never happen, implies corrupt AndroidManifest
            Log.e(TAG, "Couldn't instatiate RemotingApps", e);
            assert false;
        }
    }

    /**
     * Returns the first RemotingApp that can support the given source, or null if none are found.
     */
    public RemotingApp getRemotingApp(MediaSource source) {
        if (!(source instanceof RemotingMediaSource)) return null;

        for (RemotingApp app : mRemotingApps) {
            if (app.canPlayMedia((RemotingMediaSource) source)) {
                return app;
            }
        }
        return null;
    }

    /**
     * The singleton instance access method.
     */
    public static RemotingAppLauncher instance() {
        if (sInstance == null) {
            sInstance = new RemotingAppLauncher();
            sInstance.createRemotingApps();
        }

        return sInstance;
    }
}
