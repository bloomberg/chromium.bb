// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

/**
 * A namespace for constants to uniquely describe certain public Intents that can be used to control
 * the life cycle of CastWebContentsActivity.
 */
public class CastIntents {
    public static final String ACTION_STOP_ACTIVITY =
            "com.google.android.apps.castshell.intent.action.STOP_ACTIVITY";
    public static final String ACTION_SCREEN_OFF =
            "com.google.android.apps.castshell.intent.action.ACTION_SCREEN_OFF";
}
