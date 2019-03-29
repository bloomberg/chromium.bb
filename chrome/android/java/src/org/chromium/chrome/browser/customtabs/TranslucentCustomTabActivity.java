// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen.SplashScreenController;

/**
 * A CustomTabActivity that has a translucent theme. This is used to ensure seamless transition of
 * a splash screen from client app to a Trusted Web Activity, see {@link SplashScreenController}.
 *
 * This class is intended to be empty. Try to avoid adding code here, put it in
 * {@link SplashScreenController} or other specialized class.
 */
public class TranslucentCustomTabActivity extends CustomTabActivity {}
