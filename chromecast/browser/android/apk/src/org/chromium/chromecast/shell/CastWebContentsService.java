// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.media.AudioManager;
import android.os.IBinder;
import android.widget.Toast;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Unit;
import org.chromium.components.content_view.ContentView;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Service for "displaying" a WebContents in CastShell.
 * <p>
 * Typically, this class is controlled by CastContentWindowAndroid, which will
 * bind to this service.
 */
@JNINamespace("chromecast::shell")
public class CastWebContentsService extends Service {
    private static final String TAG = "cr_CastWebService";
    private static final boolean DEBUG = true;
    private static final int CAST_NOTIFICATION_ID = 100;

    private final Controller<Unit> mLifetimeController = new Controller<>();
    private String mInstanceId;
    private CastAudioManager mAudioManager;
    private WindowAndroid mWindow;
    private ContentViewCore mContentViewCore;
    private ContentView mContentView;

    protected void handleIntent(Intent intent) {
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        mInstanceId = intent.getData().getPath();

        WebContents webContents = CastWebContentsIntentUtils.getWebContents(intent);
        if (webContents == null) {
            Log.e(TAG, "Received null WebContents in intent.");
            return;
        }

        detachWebContentsIfAny();
        showWebContents(webContents);
    }

    @Override
    public void onDestroy() {
        if (DEBUG) Log.d(TAG, "onDestroy");

        mLifetimeController.reset();
        super.onDestroy();
    }

    @Override
    public void onCreate() {
        if (DEBUG) Log.d(TAG, "onCreate");

        if (!CastBrowserHelper.initializeBrowser(getApplicationContext())) {
            Toast.makeText(this, R.string.browser_process_initialization_failed, Toast.LENGTH_SHORT)
                    .show();
            stopSelf();
        }

        mWindow = new WindowAndroid(this);
        CastAudioManager.getAudioManager(this).requestAudioFocusWhen(
                mLifetimeController, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
        mLifetimeController.set(Unit.unit());
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (DEBUG) Log.d(TAG, "onBind");

        handleIntent(intent);

        return null;
    }

    // Sets webContents to be the currently displayed webContents.
    // TODO(thoren): Notification.Builder(Context) is deprecated in O. Use the (Context, String)
    // constructor when CastWebContentsService starts supporting O.
    @SuppressWarnings("deprecation")
    private void showWebContents(WebContents webContents) {
        if (DEBUG) Log.d(TAG, "showWebContents");

        Notification notification = new Notification.Builder(this).build();
        startForeground(CAST_NOTIFICATION_ID, notification);

        mContentView = ContentView.createContentView(this, webContents);
        // TODO(derekjchow): productVersion
        mContentViewCore = ContentViewCore.create(this, "", webContents,
                ViewAndroidDelegate.createBasicDelegate(mContentView), mContentView, mWindow);
        // Enable display of current webContents.
        webContents.onShow();
    }

    // Remove the currently displayed webContents. no-op if nothing is being displayed.
    private void detachWebContentsIfAny() {
        if (DEBUG) Log.d(TAG, "detachWebContentsIfAny");

        stopForeground(true /*removeNotification*/);

        if (mContentView != null) {
            mContentView = null;
            mContentViewCore = null;

            // Inform CastContentWindowAndroid we're detaching.
            CastWebContentsComponent.onComponentClosed(mInstanceId);
        }
    }
}
