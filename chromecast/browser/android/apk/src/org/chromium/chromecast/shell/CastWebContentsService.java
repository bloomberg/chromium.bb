// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.net.Uri;
import android.os.IBinder;
import android.widget.Toast;

import org.chromium.base.Log;
import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Function;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.Observers;
import org.chromium.content.browser.MediaSessionImpl;
import org.chromium.content_public.browser.WebContents;

/**
 * Service for "displaying" a WebContents in CastShell.
 * <p>
 * Typically, this class is controlled by CastContentWindowAndroid, which will bind to this
 * service via CastWebContentsComponent.
 */
public class CastWebContentsService extends Service {
    private static final String TAG = "cr_CastWebService";
    private static final boolean DEBUG = true;
    private static final int CAST_NOTIFICATION_ID = 100;

    private final Controller<Intent> mIntentState = new Controller<>();
    private final Observable<WebContents> mWebContentsState =
            mIntentState.map(CastWebContentsIntentUtils::getWebContents);
    // Allows tests to inject a mock MediaSessionImpl to test audio focus logic.
    private Function<WebContents, MediaSessionImpl> mMediaSessionGetter =
            MediaSessionImpl::fromWebContents;

    {
        // React to web contents by presenting them in a headless view.
        mWebContentsState.watch(CastWebContentsView.withoutLayout(this));
        mWebContentsState.watch(x -> {
            // TODO(thoren): Notification.Builder(Context) is deprecated in O. Use the
            // (Context, String) constructor when CastWebContentsService starts supporting O.
            Notification notification = new Notification.Builder(this).build();
            startForeground(CAST_NOTIFICATION_ID, notification);
            return () -> stopForeground(true /*removeNotification*/);
        });
        mWebContentsState.map(this ::getMediaSessionImpl)
                .watch(Observers.onEnter(MediaSessionImpl::requestSystemAudioFocus));
        // Inform CastContentWindowAndroid we're detaching.
        Observable<String> instanceIdState = mIntentState.map(Intent::getData).map(Uri::getPath);
        instanceIdState.watch(Observers.onExit(CastWebContentsComponent::onComponentClosed));

        if (DEBUG) {
            mWebContentsState.watch(x -> {
                Log.d(TAG, "show web contents");
                return () -> Log.d(TAG, "detach web contents");
            });
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (DEBUG) Log.d(TAG, "onCreate");
        if (!CastBrowserHelper.initializeBrowser(getApplicationContext())) {
            Toast.makeText(this, R.string.browser_process_initialization_failed, Toast.LENGTH_SHORT)
                    .show();
            stopSelf();
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (DEBUG) Log.d(TAG, "onBind");
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        mIntentState.set(intent);
        return null;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        if (DEBUG) Log.d(TAG, "onUnbind");
        mIntentState.reset();
        return false;
    }

    private MediaSessionImpl getMediaSessionImpl(WebContents webContents) {
        return mMediaSessionGetter.apply(webContents);
    }

    @RemovableInRelease
    Observable<WebContents> observeWebContentsStateForTesting() {
        return mWebContentsState;
    }

    @RemovableInRelease
    void setMediaSessionImplGetterForTesting(Function<WebContents, MediaSessionImpl> getter) {
        mMediaSessionGetter = getter;
    }
}
