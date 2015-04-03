// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.view.accessibility.CaptioningManager;

import org.chromium.content.browser.ContentViewCore;

import java.util.Locale;

/**
 * This is the implementation of SystemCaptioningBridge that uses CaptioningManager
 * on KitKat+ systems.
 */
@TargetApi(Build.VERSION_CODES.KITKAT)
public class KitKatCaptioningBridge implements SystemCaptioningBridge {
    private final CaptioningManager.CaptioningChangeListener mCaptioningChangeListener =
            new KitKatCaptioningChangeListener();

    private final CaptioningChangeDelegate mCaptioningChangeDelegate;
    private final CaptioningManager mCaptioningManager;

    /**
     * Bridge listener to inform the mCaptioningChangeDelegate when the mCaptioningManager
     * broadcasts any changes.
     */
    private class KitKatCaptioningChangeListener extends
            CaptioningManager.CaptioningChangeListener {
        @Override
        public void onEnabledChanged(boolean enabled) {
            mCaptioningChangeDelegate.onEnabledChanged(enabled);
        }

        @Override
        public void onFontScaleChanged(float fontScale) {
            mCaptioningChangeDelegate.onFontScaleChanged(fontScale);
        }

        @Override
        public void onLocaleChanged(Locale locale) {
            mCaptioningChangeDelegate.onLocaleChanged(locale);
        }

        @Override
        public void onUserStyleChanged(CaptioningManager.CaptionStyle userStyle) {
            final CaptioningStyle captioningStyle = getCaptioningStyleFrom(userStyle);
            mCaptioningChangeDelegate.onUserStyleChanged(captioningStyle);
        }
    }

    /**
     * Construct a new KitKat+ captioning bridge
     *
     * @param contentViewCore the ContentViewCore to associate with this bridge.
     */
    public KitKatCaptioningBridge(ContentViewCore contenViewCore) {
        mCaptioningChangeDelegate = new CaptioningChangeDelegate(contenViewCore);
        mCaptioningManager = (CaptioningManager) contenViewCore.getContext().getSystemService(
                Context.CAPTIONING_SERVICE);
        mCaptioningManager.addCaptioningChangeListener(mCaptioningChangeListener);
        syncToDelegate();
    }

    /**
     * Force-sync the current closed caption settings to the delegate
     */
    public void syncToDelegate() {
        mCaptioningChangeDelegate.onEnabledChanged(mCaptioningManager.isEnabled());
        mCaptioningChangeDelegate.onFontScaleChanged(mCaptioningManager.getFontScale());
        mCaptioningChangeDelegate.onLocaleChanged(mCaptioningManager.getLocale());
        mCaptioningChangeDelegate.onUserStyleChanged(
                getCaptioningStyleFrom(mCaptioningManager.getUserStyle()));
    }

    /**
     * De-register this bridge from the system captioning manager. This bridge
     * should not be used again after this is called.
     */
    public void destroy() {
        mCaptioningManager.removeCaptioningChangeListener(mCaptioningChangeListener);
    }

    /**
     * Create a Chromium CaptioningStyle from a platform CaptionStyle
     *
     * @param userStyle the platform CaptionStyle
     * @return a Chromium CaptioningStyle
     *
     */
    private CaptioningStyle getCaptioningStyleFrom(CaptioningManager.CaptionStyle userStyle) {
        return CaptioningStyle.createFrom(userStyle);
    }
}
