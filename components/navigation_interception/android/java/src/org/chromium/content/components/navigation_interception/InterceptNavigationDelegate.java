// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.navigation_interception;

import org.chromium.base.CalledByNative;

public interface InterceptNavigationDelegate {
    /**
     * This method is called for every top-level navigation within the associated WebContents.
     * The method allows the embedder to ignore navigations. This is used on Android to 'convert'
     * certain navigations to Intents to 3rd party applications.
     *
     * @param url the target url of the navigation.
     * @param isPost true if the the navigation method is "POST".
     * @param isUserGesture true if the navigation was initiated by the user.
     * @param pageTransition is the page transition type (e.g. link / typed).
     * @return true if the navigation should be ignored.
     */
    @CalledByNative
    boolean shouldIgnoreNavigation(String url, boolean isPost, boolean isUserGesture,
                                   int pageTransition);
}
