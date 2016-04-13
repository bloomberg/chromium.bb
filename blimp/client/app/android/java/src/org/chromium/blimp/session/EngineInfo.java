// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.session;

/**
 * Stores the information about the engine.
 */
public class EngineInfo {
    public final String ipAddress;
    public final String engineVersion;
    public final String assignerUrl;
    private boolean mConnected;

    public EngineInfo(String ip, String version, String assigner) {
        ipAddress = ip;
        engineVersion = version;
        assignerUrl = assigner;
    }

    public boolean isConnected() {
        return mConnected;
    }

    public void setConnected(boolean connected) {
        this.mConnected = connected;
    }
}
