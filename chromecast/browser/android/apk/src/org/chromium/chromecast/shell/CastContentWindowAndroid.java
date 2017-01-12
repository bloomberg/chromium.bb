// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.PatternMatcher;
import android.support.v4.content.LocalBroadcastManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * The Java component of CastContentWindowAndroid. This class is responsible for
 * starting, stopping and monitoring CastWebContentsActivity.
 *
 * See chromecast/browser/cast_content_window_android.* for the native half.
 */
@JNINamespace("chromecast::shell")
public class CastContentWindowAndroid {
    private static final String TAG = "cr_CastContentWindowAndroid";
    private static final boolean DEBUG = true;

    // Note: CastContentWindowAndroid may outlive the native object. The native
    // ref should be checked that it is not zero before it is used.
    private long mNativeCastContentWindowAndroid;
    private Context mContext;
    private IntentFilter mActivityClosedIntentFilter;
    private BroadcastReceiver mActivityClosedBroadcastReceiver;
    private String mInstanceId;

    private static int sInstanceId = 1;

    @SuppressWarnings("unused")
    @CalledByNative
    private static CastContentWindowAndroid create(long nativeCastContentWindowAndroid) {
        return new CastContentWindowAndroid(
                nativeCastContentWindowAndroid, ContextUtils.getApplicationContext());
    }

    private CastContentWindowAndroid(long nativeCastContentWindowAndroid, Context context) {
        mNativeCastContentWindowAndroid = nativeCastContentWindowAndroid;
        mContext = context;
        mInstanceId = Integer.toString(sInstanceId++);
    }

    private Uri getInstanceUri() {
        Uri instanceUri = new Uri.Builder()
                .scheme(CastWebContentsActivity.ACTION_DATA_SCHEME)
                .authority(CastWebContentsActivity.ACTION_DATA_AUTHORITY)
                .path(mInstanceId)
                .build();
        return instanceUri;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void showWebContents(WebContents webContents) {
        if (DEBUG) Log.d(TAG, "showWebContents");

        Intent intent = new Intent(
                Intent.ACTION_VIEW, getInstanceUri(), mContext, CastWebContentsActivity.class);

        mActivityClosedBroadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent.getAction() == CastWebContentsActivity.ACTION_ACTIVITY_STOPPED) {
                    onActivityStopped();
                } else if (intent.getAction() == CastWebContentsActivity.ACTION_KEY_EVENT) {
                    int keyCode =
                            intent.getIntExtra(CastWebContentsActivity.ACTION_EXTRA_KEY_CODE, 0);
                    onKeyDown(keyCode);
                }
            }
        };
        mActivityClosedIntentFilter = new IntentFilter();
        mActivityClosedIntentFilter.addDataScheme(intent.getData().getScheme());
        mActivityClosedIntentFilter.addDataAuthority(intent.getData().getAuthority(), null);
        mActivityClosedIntentFilter.addDataPath(
                intent.getData().getPath(), PatternMatcher.PATTERN_LITERAL);
        mActivityClosedIntentFilter.addAction(CastWebContentsActivity.ACTION_ACTIVITY_STOPPED);
        mActivityClosedIntentFilter.addAction(CastWebContentsActivity.ACTION_KEY_EVENT);
        LocalBroadcastManager.getInstance(mContext).registerReceiver(
                mActivityClosedBroadcastReceiver, mActivityClosedIntentFilter);

        intent.putExtra(CastWebContentsActivity.ACTION_EXTRA_WEB_CONTENTS, webContents);
        // FLAG_ACTIVITY_SINGLE_TOP will try to reuse existing activity.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK
                | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        mContext.startActivity(intent);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onNativeDestroyed() {
        assert mNativeCastContentWindowAndroid != 0;
        mNativeCastContentWindowAndroid = 0;

        // Note: there is a potential race condition when this function is called after
        // showWebContents. If the stop intent is received after the start intent but before
        // onCreate, the activity won't shutdown.
        // TODO(derekjchow): Add a unittest to check this behaviour. Also consider using
        // Instrumentation.startActivitySync to guarentee onCreate is run.

        if (DEBUG) Log.d(TAG, "onNativeDestroyed");
        Intent intent = new Intent(CastWebContentsActivity.ACTION_STOP_ACTIVITY, getInstanceUri());
        LocalBroadcastManager.getInstance(mContext).sendBroadcastSync(intent);
    }

    private void onActivityStopped() {
        if (DEBUG) Log.d(TAG, "onActivityStopped");
        if (mNativeCastContentWindowAndroid != 0) {
            nativeOnActivityStopped(mNativeCastContentWindowAndroid);
        }
    }

    private void onKeyDown(int keyCode) {
        if (DEBUG) Log.d(TAG, "onKeyDown");
        if (mNativeCastContentWindowAndroid != 0) {
            nativeOnKeyDown(mNativeCastContentWindowAndroid, keyCode);
        }
    }

    private native void nativeOnActivityStopped(long nativeCastContentWindowAndroid);
    private native void nativeOnKeyDown(long nativeCastContentWindowAndroid, int keyCode);
}
