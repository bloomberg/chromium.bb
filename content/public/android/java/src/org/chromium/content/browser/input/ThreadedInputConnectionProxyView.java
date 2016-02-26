// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.os.Handler;
import android.os.IBinder;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.concurrent.Callable;

/**
 * This is a fake View that is only exposed to InputMethodManager.
 */
public class ThreadedInputConnectionProxyView extends View {
    private static final String TAG = "cr_Ime";
    private static final boolean DEBUG_LOGS = false;

    private final Handler mHandler;
    private final View mContainerView;

    ThreadedInputConnectionProxyView(Context context, Handler handler, View containerView) {
        super(context);
        mHandler = handler;
        mContainerView = containerView;
        setFocusable(true);
        setFocusableInTouchMode(true);
        setVisibility(View.VISIBLE);
        if (DEBUG_LOGS) Log.w(TAG, "constructor");
    }

    @Override
    public Handler getHandler() {
        if (DEBUG_LOGS) Log.w(TAG, "getHandler");
        return mHandler;
    }

    @Override
    public boolean checkInputConnectionProxy(View view) {
        if (DEBUG_LOGS) Log.w(TAG, "checkInputConnectionProxy");
        return mContainerView == view;
    }

    @Override
    public InputConnection onCreateInputConnection(final EditorInfo outAttrs) {
        if (DEBUG_LOGS) Log.w(TAG, "onCreateInputConnection");
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<InputConnection>() {
            @Override
            public InputConnection call() throws Exception {
                return mContainerView.onCreateInputConnection(outAttrs);
            }
        });
    }

    @Override
    public boolean hasFocus() {
        if (DEBUG_LOGS) Log.w(TAG, "hasFocus");
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContainerView.hasFocus();
            }
        });
    }

    @Override
    public boolean hasWindowFocus() {
        if (DEBUG_LOGS) Log.w(TAG, "hasWindowFocus");
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContainerView.hasWindowFocus();
            }
        });
    }

    @Override
    public View getRootView() {
        if (DEBUG_LOGS) Log.w(TAG, "getRootView");
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<View>() {
            @Override
            public View call() throws Exception {
                return mContainerView.getRootView();
            }
        });
    }

    @Override
    public boolean onCheckIsTextEditor() {
        if (DEBUG_LOGS) Log.w(TAG, "onCheckIsTextEditor");
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContainerView.onCheckIsTextEditor();
            }
        });
    }

    @Override
    public boolean isFocused() {
        if (DEBUG_LOGS) Log.w(TAG, "isFocused");
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mContainerView.isFocused();
            }
        });
    }

    @Override
    public IBinder getWindowToken() {
        if (DEBUG_LOGS) Log.w(TAG, "getWindowToken");
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<IBinder>() {
            @Override
            public IBinder call() throws Exception {
                return mContainerView.getWindowToken();
            }
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (DEBUG_LOGS) Log.w(TAG, "onWindowFocusChanged:" + hasWindowFocus);
        super.onWindowFocusChanged(hasWindowFocus);
    }
}