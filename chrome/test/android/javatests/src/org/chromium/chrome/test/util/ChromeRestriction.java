// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

/**
 * Restrictions that are usable with the @Restriction enum but in the chrome/ layer.
 * e.g. @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE})
 */
public final class ChromeRestriction {
    /** Specifies the test is only valid on a device that has up to date play services. */
    public static final String RESTRICTION_TYPE_GOOGLE_PLAY_SERVICES = "Google_Play_Services";
    /** Specifies the test is only valid on phone form factors. */
    public static final String RESTRICTION_TYPE_PHONE = "Phone";
    /** Specifies the test is only valid on tablet form factors. */
    public static final String RESTRICTION_TYPE_TABLET = "Tablet";
}
