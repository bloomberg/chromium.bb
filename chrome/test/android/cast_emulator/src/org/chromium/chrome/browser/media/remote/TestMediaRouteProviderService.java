// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.support.v7.media.MediaRouteProvider;
import android.support.v7.media.MediaRouteProviderService;

/**
 * Demonstrates how to register a custom media route provider service
 * using the support library.
 *
 * @see TestMediaRouteProvider
 */
public class TestMediaRouteProviderService extends MediaRouteProviderService {
    @Override
    public MediaRouteProvider onCreateMediaRouteProvider() {
        return new TestMediaRouteProvider(this);
    }
}
