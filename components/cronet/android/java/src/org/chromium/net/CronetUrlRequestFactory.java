// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.util.concurrent.Executor;

/**
 * UrlRequest factory using Chromium HTTP stack implementation.
 */
public class CronetUrlRequestFactory implements UrlRequestFactory {
    @Override
    public UrlRequest createRequest(String url, UrlRequestListener listener,
                                    Executor executor) {
        return new CronetUrlRequest();
    }
}
