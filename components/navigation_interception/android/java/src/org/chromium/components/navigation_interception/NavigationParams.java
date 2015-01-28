// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.navigation_interception;

import org.chromium.base.CalledByNative;

public class NavigationParams {
    // Target url of the navigation.
    public final String url;
    // True if the the navigation method is "POST".
    public final boolean isPost;
    // True if the navigation was initiated by the user.
    public final boolean hasUserGesture;
    // Page transition type (e.g. link / typed).
    public final int pageTransitionType;
    // Is the navigation a redirect (in which case url is the "target" address).
    public final boolean isRedirect;
    // True if the target url can't be handled by Chrome's internal protocol handlers.
    public final boolean isExternalProtocol;

    public NavigationParams(String url, boolean isPost, boolean hasUserGesture,
            int pageTransitionType, boolean isRedirect, boolean isExternalProtocol) {
        this.url = url;
        this.isPost = isPost;
        this.hasUserGesture = hasUserGesture;
        this.pageTransitionType = pageTransitionType;
        this.isRedirect = isRedirect;
        this.isExternalProtocol = isExternalProtocol;
    }

    @CalledByNative
    public static NavigationParams create(String url, boolean isPost, boolean hasUserGesture,
            int pageTransitionType, boolean isRedirect, boolean isExternalProtocol) {
        return new NavigationParams(
                url, isPost, hasUserGesture, pageTransitionType, isRedirect, isExternalProtocol);
    }
}
