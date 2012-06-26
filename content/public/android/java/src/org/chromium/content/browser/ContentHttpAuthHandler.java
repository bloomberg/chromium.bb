// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

/**
 * This is an interface that is used for HTTP authentication requests handled by the UI. The default
 * implementation just needs to override the method that cancels the request. ContentViewClient
 * uses a callback that receives an object of type ContentHttpAuthHandler to communicate the request
 * to the java side.
 */
public interface ContentHttpAuthHandler {
    /**
     * A class that implements this interface should have at the least the functionality to cancel
     * the authentication request which is the default behaviour when a request is received.
     */
    public void cancel();
}
