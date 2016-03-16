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
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.OfflinePageModelObserver;
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
    private static final String TEST_URL = "TEST_URL";
    private static final long TEST_OFFLINE_ID = 42;
    private static final ClientId TEST_CLIENT_ID = new ClientId(TEST_NAMESPACE, TEST_ID);
    private static final String TEST_OFFLINE_URL = "TEST_OFFLINE_URL";
    private static final long TEST_FILESIZE = 12345;
    private static final long TEST_CREATIONTIMEMS = 150;
    private static final int TEST_ACCESSCOUNT = 1;
    private static final long TEST_LASTACCESSTIMEMS = 20160314;

    private static final OfflinePageItem TEST_OFFLINE_PAGE_ITEM = new OfflinePageItem(TEST_URL,
            TEST_OFFLINE_ID, TEST_NAMESPACE, TEST_ID, TEST_OFFLINE_URL, TEST_FILESIZE,
            TEST_CREATIONTIMEMS, TEST_ACCESSCOUNT, TEST_LASTACCESSTIMEMS);

    /**
     * Mocks the observer.
     */
    public class MockOfflinePageModelObserver extends OfflinePageModelObserver {
        public long lastDeletedOfflineId;
        public ClientId lastDeletedClientId;

        public void offlinePageDeleted(long offlineId, ClientId clientId) {
            lastDeletedOfflineId = offlineId;
            lastDeletedClientId = clientId;
        }
    }

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

    /**
     * Tests getClientIdForOfflineId.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testGetClientIdForOfflineId() {
        doReturn(TEST_OFFLINE_PAGE_ITEM)
                .when(mBridge)
                .nativeGetPageByOfflineId(anyLong(), eq(TEST_OFFLINE_ID));

        mBridge.offlinePageModelLoaded();
        long testOfflineId = TEST_OFFLINE_ID;
        ClientId resultClientId = mBridge.getClientIdForOfflineId(testOfflineId);
        assertEquals(resultClientId, TEST_CLIENT_ID);
        verify(mBridge, times(1)).getClientIdForOfflineId(eq(TEST_OFFLINE_ID));
        verify(mBridge, times(1)).nativeGetPageByOfflineId(anyLong(), eq(TEST_OFFLINE_ID));
    }

    /**
     * Tests getClientIdForOfflineId for null.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testGetClientIdForOfflineIdNull() {
        doReturn(null).when(mBridge).nativeGetPageByOfflineId(anyLong(), eq(TEST_OFFLINE_ID));
        mBridge.offlinePageModelLoaded();
        long testOfflineId = TEST_OFFLINE_ID;
        ClientId resultClientId = mBridge.getClientIdForOfflineId(testOfflineId);
        assertEquals(resultClientId, null);
        verify(mBridge, times(1)).getClientIdForOfflineId(eq(TEST_OFFLINE_ID));
        verify(mBridge, times(1)).nativeGetPageByOfflineId(anyLong(), eq(TEST_OFFLINE_ID));
    }

    /**
     * Tests OfflinePageBridge#OfflinePageDeleted() callback with two observers attached.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testRemovePageByClientId() {
        MockOfflinePageModelObserver observer1 = new MockOfflinePageModelObserver();
        MockOfflinePageModelObserver observer2 = new MockOfflinePageModelObserver();
        mBridge.addObserver(observer1);
        mBridge.addObserver(observer2);

        ClientId testClientId = new ClientId(TEST_NAMESPACE, TEST_ID);
        long testOfflineId = 123;
        mBridge.offlinePageDeleted(testOfflineId, testClientId);
        assertEquals(testOfflineId, observer1.lastDeletedOfflineId);
        assertEquals(testClientId, observer1.lastDeletedClientId);
        assertEquals(testOfflineId, observer2.lastDeletedOfflineId);
        assertEquals(testClientId, observer2.lastDeletedClientId);
    }

    @Test(expected = AssertionError.class)
    @Feature({"OfflinePages"})
    public void testGetPageByClientId_ModelNotLoaded() {
        ClientId testClientId = new ClientId("TEST_NAMESPACE", "TEST_ID");
        Set<Long> result = mBridge.getOfflineIdsForClientId(testClientId);
    }
}
