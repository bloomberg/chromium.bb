// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * CookieManager manages cookies according to RFC2109 spec.
 *
 * Methods in this class are thread safe.
 */
public final class AwCookieManager extends CookieManager {
    private native void nativeForJni();
}
