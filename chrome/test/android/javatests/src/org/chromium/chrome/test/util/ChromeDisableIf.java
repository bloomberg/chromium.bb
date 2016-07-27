// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

/**
 * Disable if enums that are usable with the @DisableIf but in the chrome/ layer.
 *
 * e.g. @DisableIf.Device({ChromeDisableIf.PHONE})
 */
public final class ChromeDisableIf {
    /** Specifies the test is disabled if on phone form factors. */
    public static final String PHONE = "Phone";
    /** Specifies the test is disabled if on tablet form factors. */
    public static final String TABLET = "Tablet";
    /** Specifies the test is disabled if on large tablet form factors. */
    public static final String LARGETABLET = "LargeTablet";
}
