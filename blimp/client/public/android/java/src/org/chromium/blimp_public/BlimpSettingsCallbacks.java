// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp_public;

/**
 * Callback interface for Blimp to call functions in Chrome from Blimp settings.
 * Chrome should implements this interface and pass it to Blimp via BlimpClientContext.
 */
public interface BlimpSettingsCallbacks {

    /**
     * Request Chrome to restart browser.
     */
    public void onRestartBrowserRequested();
}
