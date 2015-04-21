// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

/**
 * This is the empty implementation of SystemCaptioningBridge that does nothing used
 * on versions of Android that do not have support for system closed captioning settings.
 */
public class EmptyCaptioningBridge implements SystemCaptioningBridge {
    /**
     * A no-op implementation of the syncToDelegate function.
     */
    @Override
    public void syncToDelegate() {}

    /**
     * No-op implementation of registerBridge.
     */
    @Override
    public void registerBridge() {}

    /**
     * A no-op implementation of the unregisterBridge function.
     */
    @Override
    public void unregisterBridge() {}
}
