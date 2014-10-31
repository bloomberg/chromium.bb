// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

/**
 * Remote DevTools bridge instance (for testing app).
 */
public final class RemoteInstance {
    public final String id;
    public final String displayName;

    public RemoteInstance(String id, String displayName) {
        assert id != null;
        assert displayName != null;

        this.id = id;
        this.displayName = displayName;
    }

    @Override
    public String toString() {
        return displayName + ": " + id;
    }
}
