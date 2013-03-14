// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.util.Locale;

/**
 * This class provides the locale related methods for the native library.
 */
public class LocaleUtils {

    private LocaleUtils() { /* cannot be instantiated */ }

    /**
     * @return the default locale, translating Android deprecated
     * language codes into the modern ones used by Chromium.
     */
    @CalledByNative
    public static String getDefaultLocale() {
        Locale locale = Locale.getDefault();
        String language = locale.getLanguage();
        String country = locale.getCountry();

        // Android uses deprecated lanuages codes for Hebrew, Indonesian and
        // Yiddish but Chromium uses the updated codes, so apply a mapping.
        // See http://developer.android.com/reference/java/util/Locale.html
        if ("iw".equals(language)) {
            language = "he";
        } else if ("in".equals(language)) {
            language = "id";
        } else if ("ji".equals(language)) {
            language = "yi";
        }
        return country.isEmpty() ? language : language + "-" + country;
    }

    @CalledByNative
    private static Locale getJavaLocale(String language, String country, String variant) {
        return new Locale(language, country, variant);
    }

    @CalledByNative
    private static String getDisplayNameForLocale(Locale locale, Locale displayLocale) {
        return locale.getDisplayName(displayLocale);
    }
}
