// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/**
 * Listener class for receiving the status information of a request.
 */
public abstract class StatusListener {
    /**
     * Called on {@link UrlRequest}'s {@link java.util.concurrent.Executor}'s
     * thread when request status is obtained.
     * @param status integer representing the status of the request. It is
     *         one of the values defined in {@link RequestStatus}.
     */
    public abstract void onStatus(int status);
}
