// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

/**
 * Interface for platform dependent captioning bridges.
 */
public interface SystemCaptioningBridge {
    /**
     * Calls all of the delegate's methods with the system's current captioning settings.
     */
    public void syncToDelegate();

    /**
     * Register this bridge for event changes with the system CaptioningManager.
     */
    public void registerBridge();

    /**
     * Unregister this bridge from system CaptionManager. Must be called to avoid leaks.
     */
    public void unregisterBridge();
}
