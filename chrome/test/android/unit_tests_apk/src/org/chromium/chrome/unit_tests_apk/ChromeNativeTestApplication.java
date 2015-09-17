// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.unit_tests_apk;

import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.smartcard.PKCS11AuthenticationManager;

/**
 * A stub implementation of the chrome application to be used in chrome unit_tests.
 */
public class ChromeNativeTestApplication extends ChromeApplication {

    @Override
    protected boolean areParentalControlsEnabled() {
        return false;
    }

    @Override
    protected PKCS11AuthenticationManager getPKCS11AuthenticationManager() {
        return null;
    }

    @Override
    public void initCommandLine() {
    }
}
