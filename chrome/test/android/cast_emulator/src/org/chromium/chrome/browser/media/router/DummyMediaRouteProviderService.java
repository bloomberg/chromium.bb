// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.support.v7.media.MediaRouteProvider;
import android.support.v7.media.MediaRouteProviderService;

import org.chromium.base.Log;

/**
 * Service for registering {@link DummyMediaRouteProvider} using the support library.
 */
public class DummyMediaRouteProviderService extends MediaRouteProviderService {
    private static final String TAG = "DummyMRP";

    @Override
    public MediaRouteProvider onCreateMediaRouteProvider() {
        Log.i(TAG, "creating DummyMRP");
        return new DummyMediaRouteProvider(this);
    }
}
