// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import org.chromium.chrome.browser.EmbedContentViewActivity;

/**
 * Staging version of FirstRunActivity. This class should be merged into FirstRunActivity
 * once it's upstreamed.
 */
public class FirstRunActivityStaging extends FirstRunActivity {
    @Override
    public void showEmbedContentViewActivity(int title, int url) {
        EmbedContentViewActivity.show(this, title, url);
    }
}
