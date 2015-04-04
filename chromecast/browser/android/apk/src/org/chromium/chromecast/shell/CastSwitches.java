// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

/**
 * Contains command line switches specific to Cast on Android.
 */
public final class CastSwitches {
    // Default height to start an application at, unless the application explicitly requests
    // a different height. If unspecified, this defaults to 720px.
    public static final String DEFAULT_RESOLUTION_HEIGHT = "default-resolution-height";
}
