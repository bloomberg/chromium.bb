// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.util.Set;

/**
 * Unit tests for OfflinePageUtils.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, application = BaseChromiumApplication.class)
public class OfflinePageBridgeTest {
    private OfflinePageBridge mBridge;

    private static final String TEST_NAMESPACE = "TEST_NAMESPACE";
    private static final String TEST_ID = "TEST_ID";

    @Before
    public void setUp() throws Exception {
        OfflinePageBridge bridge = new OfflinePageBridge(0);
        // Using the spy to automatically marshal all the calls to the original methods if they are
        // not mocked explicitly.
        mBridge = spy(bridge);
    }

    /**
     * Tests OfflinePageBridge#getPageByClientId() method in a scenario where a model was loaded.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testGetPageByClientId() {
        doReturn(new long[] {123, 456})
                .when(mBridge)
                .nativeGetOfflineIdsForClientId(anyLong(), eq(TEST_NAMESPACE), eq(TEST_ID));

        mBridge.offlinePageModelLoaded();

        ClientId testClientId = new ClientId(TEST_NAMESPACE, TEST_ID);
        Set<Long> result = mBridge.getOfflineIdsForClientId(testClientId);
        assertEquals(2, result.size());
        assertTrue(result.contains(123L));
        assertTrue(result.contains(456L));
        verify(mBridge, times(1))
                .nativeGetOfflineIdsForClientId(anyLong(), anyString(), anyString());
    }

    @Test(expected = AssertionError.class)
    @Feature({"OfflinePages"})
    public void testGetPageByClientId_ModelNotLoaded() {
        ClientId testClientId = new ClientId("TEST_NAMESPACE", "TEST_ID");
        Set<Long> result = mBridge.getOfflineIdsForClientId(testClientId);
    }
}
