// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.webauth.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.webauth.mojom.PublicKeyCredentialRequestOptions;

/**
 * Android implementation of the Authenticator service defined in
 * components/webauth/authenticator.mojom.
 */
public class U2fApiHandler {
    private static U2fApiHandler sInstance;

    /**
     * @return The U2fApiHandler for use during the lifetime of the browser process.
     */
    public static U2fApiHandler getInstance() {
        ThreadUtils.checkUiThread();
        if (sInstance == null) {
            sInstance = AppHooks.get().createU2fApiHandler();
        }
        return sInstance;
    }

    protected void u2fRegister(
            PublicKeyCredentialCreationOptions options, HandlerResponseCallback callback) {}

    protected void u2fSign(
            PublicKeyCredentialRequestOptions options, HandlerResponseCallback callback) {}
}
