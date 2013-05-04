// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import org.chromium.base.CalledByNative;

/**
 * Java representation of native StatusTray.
 */
public class StatusTray {

    private static StatusTrayFactory sFactory;

    public StatusTray() {
    }

    /**
     * Sets the Factory instance.
     * @param factory The factory instance to set.
     */
    public static void setFactory(StatusTrayFactory factory) {
        sFactory = factory;
    }

    /**
     * Returns a new instance of StatusTray created by a factory instance.
     * @param context Current application context.
     */
    @CalledByNative
    private static StatusTray create(Context context) {
        assert sFactory != null;
        return sFactory.create(context);
    }

    /**
     * Creates the notification to indicate media capture has started.
     */
    @CalledByNative
    protected void createMediaCaptureStatusNotification() {
    }

    /**
     * A factory class to create a StatusTray.
     */
    public static class StatusTrayFactory {
        /**
         * Returns a new instance of StatusTray.
         * @param context Current application context.
         */
        public StatusTray create(Context context) {
            return new StatusTray();
        }
    }

}
