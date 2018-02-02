// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link OfflinePageOrigin}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OfflinePageOriginUnitTest {
    @Test
    public void testEncodeAsJson() {
        String appName = "abc.xyz";
        String[] signatures = new String[] {"deadbeef", "00c0ffee"};
        OfflinePageOrigin origin = new OfflinePageOrigin(appName, signatures);

        assertEquals("[\"abc.xyz\",[\"deadbeef\",\"00c0ffee\"]]", origin.encodeAsJsonString());
    }
}
