// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import static org.mockito.AdditionalAnswers.answerVoid;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.VoidAnswer3;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.List;

/** Tests for SendTabToSelfAndroidBridge */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SendTabToSelfAndroidBridgeTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    SendTabToSelfAndroidBridge.Natives mNativeMock;
    private Profile mProfile;

    private SendTabToSelfAndroidBridge mBridge;
    private static final String GUID = "randomguid";
    private static final String URL = "http://www.tanyastacos.com";
    private static final String TITLE = "Come try Tanya's famous tacos";
    private static final String DEVICE_NAME = "Macbook Pro";
    private static final long NAVIGATION_TIME_MS = 123l;
    private static final long SHARE_TIME_MS = 456l;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);

        mBridge = new SendTabToSelfAndroidBridge(mProfile);
        verify(mNativeMock).init(any(SendTabToSelfAndroidBridge.class), eq(mProfile));
    }

    @Test
    @SmallTest
    public void testAddEntry() {
        mBridge.addEntry(URL, TITLE);
        verify(mNativeMock)
                .addEntry(any(SendTabToSelfAndroidBridge.class), anyLong(), eq(URL), eq(TITLE));
    }

    @Test
    @SmallTest
    @SuppressWarnings("unchecked")
    public void testGetAllGuids() {
        doAnswer(answerVoid(new VoidAnswer3<SendTabToSelfAndroidBridge, Long, List<String>>() {
            @Override
            public void answer(SendTabToSelfAndroidBridge bridge, Long nativeBridgeAddress,
                    List<String> guids) {
                guids.add("one");
                guids.add("two");
                guids.add("three");
            }
        }))
                .when(mNativeMock)
                .getAllGuids(any(SendTabToSelfAndroidBridge.class), anyLong(), any(List.class));

        List<String> actual = mBridge.getAllGuids();

        verify(mNativeMock)
                .getAllGuids(any(SendTabToSelfAndroidBridge.class), anyLong(), any(List.class));
        Assert.assertEquals(3, actual.size());
        Assert.assertArrayEquals(new String[] {"one", "two", "three"}, actual.toArray());
    }

    @Test
    @SmallTest
    public void testGetEntryByGUID() {
        SendTabToSelfEntry expected = new SendTabToSelfEntry(
                GUID, URL, TITLE, SHARE_TIME_MS, NAVIGATION_TIME_MS, DEVICE_NAME);
        when(mNativeMock.getEntryByGUID(
                     any(SendTabToSelfAndroidBridge.class), anyLong(), anyString()))
                .thenReturn(expected);
        // Note that the GUID passed in this function does not match the GUID of the returned entry.
        // This is okay because the purpose of the test is to make sure that the JNI layer passes
        // the entry returned by the native code. The native code does the actual matching of
        // the GUID but since that is mocked out and not the purpose of the test, this is fine.
        SendTabToSelfEntry actual = mBridge.getEntryByGUID("guid");
        Assert.assertEquals(expected.guid, actual.guid);
        Assert.assertEquals(expected.url, actual.url);
        Assert.assertEquals(expected.title, actual.title);
        Assert.assertEquals(expected.sharedTime, actual.sharedTime);
        Assert.assertEquals(expected.originalNavigationTime, actual.originalNavigationTime);
        Assert.assertEquals(expected.deviceName, actual.deviceName);
    }

    @Test
    @SmallTest
    public void testDeleteAllEntries() {
        mBridge.deleteAllEntries();
        verify(mNativeMock).deleteAllEntries(any(SendTabToSelfAndroidBridge.class), anyLong());
    }
}
