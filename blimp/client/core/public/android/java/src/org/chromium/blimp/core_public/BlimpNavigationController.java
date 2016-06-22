// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core_public;

/**
 * BlimpNavigationController is the Java representation of a native BlimpNavigationController
 * object.
 *
 * The BlimpNavigationController maintains the back-forward list for a {@link BlimpContents} and
 * manages all navigation within that list.
 *
 * Each BlimpNavigationController belongs to one {@link BlimpContents}, and each
 * {@link BlimpContents} has exactly one BlimpNavigationController.
 */
public interface BlimpNavigationController {

    /**
     * Calls to loadUrl informs the engine that it should start navigating to the provided |url|.
     * @param url the URL to start navigation to.
     */
    void loadUrl(String url);

    /**
     * Retrieves the URL of the currently selected item in the navigation list.
     */
    String getUrl();
}
