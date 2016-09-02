// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core;

/**
 * Contains all of the command line switches that are specific to the blimp/
 * portion of Chromium on Android.
 */
public final class BlimpClientSwitches {
    // Can be used to force blimp on, bypassing the settings requirements.
    public static final String ENABLE_BLIMP = "enable-blimp";

    // Specifies the engine's IP address. Must be used in conjunction with
    // --engine-port and --engine-transport.
    // This is the same command line flag as kEngineIP in
    // blimp/client/core/blimp_client_switches.cc
    public static final String ENGINE_IP = "engine-ip";

    // Prevent instantiation.
    private BlimpClientSwitches() {}
}
