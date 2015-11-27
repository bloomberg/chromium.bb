// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import java.util.HashSet;
import java.util.Set;

/**
 * Contains the information about the Cast session associated with the route.
 */
public class SessionRecord {
    /**
     * Clients connected to this session.
     */
    public Set<String> clientIds = new HashSet<String>();

    /**
     * Routes connected to this session.
     */
    public Set<String> routeIds = new HashSet<String>();

    /**
     * The sink id this session is connected to.
     */
    public final String sinkId;

    /**
     * The session.
     */
    public final CastRouteController session;

    /**
     * Whether is session is being stopped at the moment.
     */
    public boolean isStopping = false;

    SessionRecord(String sinkId, CastRouteController session) {
        this.sinkId = sinkId;
        this.session = session;
    }
}
