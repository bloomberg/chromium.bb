// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.downloads;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link OfflinePageDownloadBridge}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, application = BaseChromiumApplication.class,
        shadows = {ShadowMultiDex.class})
public class OfflinePageDownloadBridgeTest {
    /** Object under test. Will be spied on by Mockito. */
    private OfflinePageDownloadBridge mBridge;

    @Mock
    private OfflinePageDownloadBridge.Observer mObserver1;
    @Mock
    private OfflinePageDownloadBridge.Observer mObserver2;
    @Mock
    private Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        OfflinePageDownloadBridge.setIsTesting(true);
        OfflinePageDownloadBridge bridge = new OfflinePageDownloadBridge(mProfile);
        mBridge = spy(bridge);
    }

    @Test
    @Feature({"OfflinePages"})
    public void testAddObserver() {
        mBridge.addObserver(mObserver1);
        mBridge.downloadItemsLoaded();
        verify(mObserver1, times(1)).onItemsLoaded();
        mBridge.addObserver(mObserver2);
        verify(mObserver2, times(1)).onItemsLoaded();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testRemoveObserver() {
        mBridge.addObserver(mObserver1);
        mBridge.addObserver(mObserver2);
        mBridge.removeObserver(mObserver1);
        mBridge.downloadItemsLoaded();
        verify(mObserver1, times(0)).onItemsLoaded();
        verify(mObserver2, times(1)).onItemsLoaded();
    }

    @Test
    @Feature({"OfflinePages"})
    public void testObserverItemAdded() {
        mBridge.addObserver(mObserver1);
        mBridge.addObserver(mObserver2);
        OfflinePageDownloadItem item = createDownloadItem1();
        mBridge.downloadItemAdded(item);
        verify(mObserver1, times(1)).onItemAdded(eq(item));
        verify(mObserver2, times(1)).onItemAdded(eq(item));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testObserverItemUpdated() {
        mBridge.addObserver(mObserver1);
        mBridge.addObserver(mObserver2);
        OfflinePageDownloadItem item = createDownloadItem1();
        mBridge.downloadItemUpdated(item);
        verify(mObserver1, times(1)).onItemUpdated(eq(item));
        verify(mObserver2, times(1)).onItemUpdated(eq(item));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testObserverItemDeleted() {
        mBridge.addObserver(mObserver1);
        mBridge.addObserver(mObserver2);
        String guid = createDownloadItem1().getGuid();
        mBridge.downloadItemDeleted(guid);
        verify(mObserver1, times(1)).onItemDeleted(eq(guid));
        verify(mObserver2, times(1)).onItemDeleted(eq(guid));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testGetAllItems() {
        doNothing().when(mBridge).nativeGetAllItems(
                anyLong(), ArgumentMatchers.<List<OfflinePageDownloadItem>>any());
        List<OfflinePageDownloadItem> result = mBridge.getAllItems();
        verify(mBridge, times(1)).nativeGetAllItems(eq(0L), eq(result));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testGetItemByGuid() {
        OfflinePageDownloadItem item = createDownloadItem2();
        doReturn(item).when(mBridge).nativeGetItemByGuid(anyLong(), eq(item.getGuid()));
        OfflinePageDownloadItem result = mBridge.getItem(item.getGuid());
        verify(mBridge, times(1)).nativeGetItemByGuid(eq(0L), eq(item.getGuid()));
        assertEquals(result, item);
    }

    @Test
    @Feature({"OfflinePages"})
    public void testDeleteItemByGuid() {
        OfflinePageDownloadItem item = createDownloadItem1();
        doNothing().when(mBridge).nativeDeleteItemByGuid(anyLong(), eq(item.getGuid()));
        mBridge.deleteItem(item.getGuid());
        verify(mBridge, times(1)).nativeDeleteItemByGuid(eq(0L), eq(item.getGuid()));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testOpenItemByGuid() {
        OfflinePageDownloadItem item = createDownloadItem1();
        // null as item skips actual intent so no tabs are attempted to be created.
        doReturn(null).when(mBridge).nativeGetItemByGuid(anyLong(), eq(item.getGuid()));
        mBridge.openItem(item.getGuid(), null);
        verify(mBridge, times(1)).nativeGetItemByGuid(eq(0L), eq(item.getGuid()));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCreateDownloadItemAndAddToList() {
        List<OfflinePageDownloadItem> list = new ArrayList<>();
        OfflinePageDownloadItem item1 = createDownloadItem1();
        OfflinePageDownloadBridge.createDownloadItemAndAddToList(list, item1.getGuid(),
                item1.getUrl(), item1.getDownloadState(), item1.getDownloadProgressBytes(),
                item1.getTitle(), item1.getTargetPath(), item1.getStartTimeMs(),
                item1.getTotalBytes());
        assertEquals(list.size(), 1);

        OfflinePageDownloadItem item2 = createDownloadItem2();
        OfflinePageDownloadBridge.createDownloadItemAndAddToList(list, item2.getGuid(),
                item2.getUrl(),  item2.getDownloadState(), item2.getDownloadProgressBytes(),
                item2.getTitle(), item2.getTargetPath(), item2.getStartTimeMs(),
                item2.getTotalBytes());
        assertEquals(list.size(), 2);

        assertEquals(list.get(0).getGuid(), item1.getGuid());
        assertEquals(list.get(0).getUrl(), item1.getUrl());
        assertEquals(list.get(0).getDownloadState(), item1.getDownloadState());
        assertEquals(list.get(0).getDownloadProgressBytes(), item1.getDownloadProgressBytes());
        assertEquals(list.get(0).getTitle(), item1.getTitle());
        assertEquals(list.get(0).getTargetPath(), item1.getTargetPath());
        assertEquals(list.get(0).getStartTimeMs(), item1.getStartTimeMs());
        assertEquals(list.get(0).getTotalBytes(), item1.getTotalBytes());

        assertEquals(list.get(1).getGuid(), item2.getGuid());
        assertEquals(list.get(1).getUrl(), item2.getUrl());
        assertEquals(list.get(1).getDownloadState(), item2.getDownloadState());
        assertEquals(list.get(1).getDownloadProgressBytes(), item2.getDownloadProgressBytes());
        assertEquals(list.get(1).getTitle(), item2.getTitle());
        assertEquals(list.get(1).getTargetPath(), item2.getTargetPath());
        assertEquals(list.get(1).getStartTimeMs(), item2.getStartTimeMs());
        assertEquals(list.get(1).getTotalBytes(), item2.getTotalBytes());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testCreateDownloadItem() {
        OfflinePageDownloadItem item = createDownloadItem2();
        OfflinePageDownloadItem result =
                OfflinePageDownloadBridge.createDownloadItem(item.getGuid(), item.getUrl(),
                        item.getDownloadState(), item.getDownloadProgressBytes(),
                        item.getTitle(), item.getTargetPath(), item.getStartTimeMs(),
                        item.getTotalBytes());
        assertEquals(result.getGuid(), item.getGuid());
        assertEquals(result.getUrl(), item.getUrl());
        assertEquals(result.getTitle(), item.getTitle());
        assertEquals(result.getTargetPath(), item.getTargetPath());
        assertEquals(result.getStartTimeMs(), item.getStartTimeMs());
        assertEquals(result.getTotalBytes(), item.getTotalBytes());
    }

    private OfflinePageDownloadItem createDownloadItem1() {
        return new OfflinePageDownloadItem("9a4703bd-7123-4e05-ad81-f70df8934e73",
                "https://www.google.com/", 0, 153, "test title 1",
                "/storage/offline_pages/www.google.com.mhtml", 1467314220000L, 123456);
    }

    private OfflinePageDownloadItem createDownloadItem2() {
        return new OfflinePageDownloadItem("28b7dbad-7920-4ca7-809e-10ad111ef3b5",
                "https://play.google.com/", 1, 371, "test title 2",
                "/storage/offline_pages/play.google.com.mhtml", 1467408960000L, 765432);
    }

    private List<OfflinePageDownloadItem> createDownloadItemList() {
        return Arrays.asList(createDownloadItem1(), createDownloadItem2());
    }
}
