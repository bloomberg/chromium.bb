// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

/**
 * Boundary interface for SafeBrowsingResponseCompat.
 */
public interface SafeBrowsingResponseBoundaryInterface {
    public void showInterstitial(boolean allowReporting);
    public void proceed(boolean report);
    public void backToSafety(boolean report);
}
