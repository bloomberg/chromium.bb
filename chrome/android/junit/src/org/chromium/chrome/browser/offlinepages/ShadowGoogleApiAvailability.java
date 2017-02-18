// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Custom shadow for the OS's GoogleApiAvailability. */
@Implements(GoogleApiAvailability.class)
public class ShadowGoogleApiAvailability {
    @Implementation
    public int isGooglePlayServicesAvailable(Context context) {
        return ConnectionResult.SUCCESS;
    }
}
