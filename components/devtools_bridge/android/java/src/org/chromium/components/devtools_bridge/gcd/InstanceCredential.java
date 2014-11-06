// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.content.SharedPreferences;

/**
 * Information provided by GCD when instance has registered. Instance id
 * can be used for:
 * 1. Making sure incoming messages are addressed to the instance.
 * 2. It needed when sending command results to GCD.
 * 3. For device unregistration.
 *
 * The Secret supposed to be used to authenticate the instance with OAuthClient
 * (it has no user credentials).
 */
public final class InstanceCredential {
    public static final String PREF_ID = "gcd.ID";
    public static final String PREF_SECRET = "gcd.SECRET";

    public final String id;
    public final String secret;

    public InstanceCredential(String id, String secret) {
        assert id != null;
        assert secret != null;

        this.id = id;
        this.secret = secret;
    }

    public static InstanceCredential get(SharedPreferences preferences) {
        String id = preferences.getString(PREF_ID, null);
        String secret = preferences.getString(PREF_SECRET, null);
        return id != null && secret != null ? new InstanceCredential(id, secret) : null;
    }

    public void put(SharedPreferences.Editor editor) {
        editor.putString(PREF_ID, id);
        editor.putString(PREF_SECRET, secret);
    }

    public static void remove(SharedPreferences.Editor editor) {
        editor.remove(PREF_ID);
        editor.remove(PREF_SECRET);
    }
}
