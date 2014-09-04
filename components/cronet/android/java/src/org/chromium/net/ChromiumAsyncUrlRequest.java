// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/**
 * Async request using the native HTTP stack implementation.
 */
public class ChromiumAsyncUrlRequest implements AsyncUrlRequest {
    @Override
    public void setHttpMethod(String method) {

    }

    @Override
    public void addHeader(String header, String value) {

    }

    @Override
    public void start(AsyncUrlRequestListener listener) {

    }

    @Override
    public void cancel() {

    }

    @Override
    public boolean isCanceled() {
        return false;
    }

    @Override
    public void pause() {

    }

    @Override
    public boolean isPaused() {
        return false;
    }

    @Override
    public void resume() {

    }
}
