// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

/**
 * Interface providing SigninManager access to dependencies that are not part of the SignIn
 * component. This interface interacts with //chrome features such as Policy, Sync, data wiping,
 * Google Play services.
 */
public interface SigninManagerDelegate {}
