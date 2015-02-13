// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.content.browser.ContentViewClient;

/**
 * The default {@link ContentViewClient} implementation for the chrome layer embedders.
 */
public class ChromeContentViewClient extends ContentViewClient {

    @Override
    public boolean isJavascriptEnabled() {
        return PrefServiceBridge.getInstance().javaScriptEnabled();
    }

}
