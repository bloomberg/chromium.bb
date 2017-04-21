// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.everyItem;
import static org.hamcrest.Matchers.isIn;
import static org.hamcrest.Matchers.not;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Java unit tests for ChannelDefinitions.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ChannelDefinitionsTest {
    @Test
    public void testNoOverlapBetweenStartupAndLegacyChannelIds() throws Exception {
        ChannelDefinitions channelDefinitions = new ChannelDefinitions();
        assertThat(channelDefinitions.getStartupChannelIds(),
                everyItem(not(isIn(channelDefinitions.getLegacyChannelIds()))));
    }
}