// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.content.browser.SmartSelectionClient;

/**
 * A public API to enable/disable smart selection. The default is disabled.
 */
public class SmartSelectionToggle {
    public static void setEnabled(boolean enabled) {
        SmartSelectionClient.setEnabled(enabled);
    }
}
