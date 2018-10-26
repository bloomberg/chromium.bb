// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util.test;

import android.text.TextUtils;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.chrome.browser.util.UrlUtilities;

import java.util.HashMap;
import java.util.Map;

/** Implementation of UrlUtilities which does not rely on native. */
@Implements(UrlUtilities.class)
public class ShadowUrlUtilities {
    private static Map<String, String> sUrlToToDomain = new HashMap<>();

    @Implementation
    public static boolean urlsMatchIgnoringFragments(String url, String url2) {
        return TextUtils.equals(url, url2);
    }

    @Implementation
    public static String getDomainAndRegistry(String url, boolean includePrivateRegistries) {
        String domain = sUrlToToDomain.get(url);
        return domain == null ? url : domain;
    }

    /** Add a url to domain mapping. */
    public static void setUrlToToDomainMapping(String url, String domain) {
        sUrlToToDomain.put(url, domain);
    }

    /** Clear the static state. */
    public static void reset() {
        sUrlToToDomain.clear();
    }
}
