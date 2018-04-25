// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.KeyEvent;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsUserData;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.device.gamepad.GamepadList;
import org.chromium.ui.base.EventForwarder;

/**
 * Called from native to handle UI events that need access to various Java layer
 * content components.
 */
@JNINamespace("content")
public class ContentUiEventHandler {
    private final WebContentsImpl mWebContents;
    private ContentViewCore.InternalAccessDelegate mEventDelegate;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<ContentUiEventHandler> INSTANCE =
                ContentUiEventHandler::new;
    }

    public static ContentUiEventHandler fromWebContents(WebContents webContents) {
        return WebContentsUserData.fromWebContents(
                webContents, ContentUiEventHandler.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    public ContentUiEventHandler(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
        nativeInit(webContents);
    }

    public void setEventDelegate(ContentViewCore.InternalAccessDelegate delegate) {
        mEventDelegate = delegate;
    }

    private EventForwarder getEventForwarder() {
        return mWebContents.getEventForwarder();
    }

    @CalledByNative
    private boolean onKeyUp(int keyCode, KeyEvent event) {
        TapDisambiguator tapDisambiguator = TapDisambiguator.fromWebContents(mWebContents);
        if (tapDisambiguator.isShowing() && keyCode == KeyEvent.KEYCODE_BACK) {
            tapDisambiguator.backButtonPressed();
            return true;
        }
        return mEventDelegate.super_onKeyUp(keyCode, event);
    }

    @CalledByNative
    private boolean dispatchKeyEvent(KeyEvent event) {
        if (GamepadList.dispatchKeyEvent(event)) return true;
        if (!shouldPropagateKeyEvent(event)) {
            return mEventDelegate.super_dispatchKeyEvent(event);
        }

        if (ImeAdapterImpl.fromWebContents(mWebContents).dispatchKeyEvent(event)) return true;

        return mEventDelegate.super_dispatchKeyEvent(event);
    }

    /**
     * Check whether a key should be propagated to the embedder or not.
     * We need to send almost every key to Blink. However:
     * 1. We don't want to block the device on the renderer for
     * some keys like menu, home, call.
     * 2. There are no WebKit equivalents for some of these keys
     * (see app/keyboard_codes_win.h)
     * Note that these are not the same set as KeyEvent.isSystemKey:
     * for instance, AKEYCODE_MEDIA_* will be dispatched to webkit*.
     */
    private static boolean shouldPropagateKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_HOME
                || keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_CALL
                || keyCode == KeyEvent.KEYCODE_ENDCALL || keyCode == KeyEvent.KEYCODE_POWER
                || keyCode == KeyEvent.KEYCODE_HEADSETHOOK || keyCode == KeyEvent.KEYCODE_CAMERA
                || keyCode == KeyEvent.KEYCODE_FOCUS || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
                || keyCode == KeyEvent.KEYCODE_VOLUME_MUTE
                || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            return false;
        }
        return true;
    }

    private native void nativeInit(WebContents webContents);
}
