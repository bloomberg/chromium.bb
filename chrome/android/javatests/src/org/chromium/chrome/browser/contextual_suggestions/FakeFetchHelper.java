// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * A fake {@link FetchHelper} for use in testing.
 */
public class FakeFetchHelper extends FetchHelper {
    FakeFetchHelper() {
        super(null, null);
    }

    @Override
    protected void init(TabModelSelector tabModelSelector) {
        // Intentionally do nothing.
    }

    @Override
    void destroy() {
        // Intentionally do nothing.
    }
}
