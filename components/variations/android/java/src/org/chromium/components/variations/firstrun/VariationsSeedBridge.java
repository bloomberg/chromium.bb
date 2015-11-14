// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations.firstrun;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.Base64;

import org.chromium.base.annotations.CalledByNative;

/**
 * VariationsSeedBridge is a class which used to pass variations first run seed that was fetched
 * before the actual Chrome first run to Chromium core. Class provides methods to store the seed
 * in SharedPreferences and to get the seed from there. To store raw seed data class serializes
 * byte[] to Base64 encoded string and decodes this string before passing to C++ side.
 */
public final class VariationsSeedBridge {
    private static final String VARIATIONS_FIRST_RUN_SEED_BASE64 = "variations_seed_base64";
    private static final String VARIATIONS_FIRST_RUN_SEED_SIGNATURE = "variations_seed_signature";
    private static final String VARIATIONS_FIRST_RUN_SEED_COUNTRY = "variations_seed_country";

    private static String getVariationsFirstRunSeedPref(Context context, String prefName) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        return prefs.getString(prefName, "");
    }

    /**
     * Stores variations seed data (raw data, seed signature and country code) in SharedPreferences.
     */
    public static void setVariationsFirstRunSeed(
            Context context, byte[] rawSeed, String signature, String country) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        prefs.edit()
                .putString(VARIATIONS_FIRST_RUN_SEED_BASE64,
                        Base64.encodeToString(rawSeed, Base64.NO_WRAP))
                .putString(VARIATIONS_FIRST_RUN_SEED_SIGNATURE, signature)
                .putString(VARIATIONS_FIRST_RUN_SEED_COUNTRY, country)
                .apply();
    }

    @CalledByNative
    private static byte[] getVariationsFirstRunSeedData(Context context) {
        return Base64.decode(
                getVariationsFirstRunSeedPref(context, VARIATIONS_FIRST_RUN_SEED_BASE64),
                Base64.NO_WRAP);
    }

    @CalledByNative
    private static String getVariationsFirstRunSeedSignature(Context context) {
        return getVariationsFirstRunSeedPref(context, VARIATIONS_FIRST_RUN_SEED_SIGNATURE);
    }

    @CalledByNative
    private static String getVariationsFirstRunSeedCountry(Context context) {
        return getVariationsFirstRunSeedPref(context, VARIATIONS_FIRST_RUN_SEED_COUNTRY);
    }
}
