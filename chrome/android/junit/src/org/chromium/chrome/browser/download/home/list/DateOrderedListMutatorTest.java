// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.SectionHeaderListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.SeparatorViewListItem;
import org.chromium.chrome.browser.modelutil.ListObservable.ListObserver;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

import java.util.Calendar;
import java.util.Collections;
import java.util.Date;

/** Unit tests for the DateOrderedListMutator class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DateOrderedListMutatorTest {
    @Mock
    private OfflineItemFilterSource mSource;

    @Mock
    private ListObserver<Void> mObserver;

    private ListItemModel mModel;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mModel = new ListItemModel();
    }

    @After
    public void tearDown() {
        mModel = null;
    }

    /**
     * Action                               List
     * 1. Set()                             [ ]
     */
    @Test
    public void testNoItemsAndSetup() {
        when(mSource.getItems()).thenReturn(Collections.emptySet());
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        verify(mSource, times(1)).addObserver(list);

        Assert.assertEquals(0, mModel.size());
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 1:00 1/1/2018)        [ DATE    @ 0:00 1/1/2018,
     *                                        SECTION @ Video,
     *                                        item1   @ 1:00 1/1/2018 ]
     */
    @Test
    public void testSingleItem() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 1), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(2, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 1), item1);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 2:00 1/1/2018,        [ DATE    @ 0:00 1/1/2018,
     *        item2 @ 1:00 1/1/2018)          SECTION @ Video,
     *                                        item1   @ 2:00 1/1/2018,
     *                                        item2   @ 1:00 1/1/2018 ]
     */
    @Test
    public void testTwoItemsSameDay() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 2), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 1), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(3, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 2), item1);
        assertOfflineItem(mModel.get(2), buildCalendar(2018, 1, 1, 1), item2);
    }

    /**
     * Action                                     List
     * 1. Set(item1 @ 2:00 1/1/2018 Video,        [ DATE    @ 0:00 1/1/2018,
     *        item2 @ 1:00 1/1/2018 Audio)          SECTION @ Video,
     *                                              item1   @ 2:00 1/1/2018,
     *                                              -----------------------
     *                                              SECTION @ Audio,
     *                                              item2   @ 1:00 1/1/2018 ]
     */
    @Test
    public void testTwoItemsSameDayDifferentSection() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 2), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 1), OfflineItemFilter.FILTER_AUDIO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(5, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 2), item1);
        assertSeparator(mModel.get(2), buildCalendar(2018, 1, 1, 0), false);
        assertSectionHeader(
                mModel.get(3), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_AUDIO, false);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 1, 1), item2);
    }

    /**
     * Action                                     List
     * 1. Set(item1 @ 2:00 1/1/2018 Video,        [ DATE    @ 0:00 1/1/2018,
     *        item2 @ 1:00 1/1/2018 Image)          SECTION @ Video,
     *                                              item1   @ 2:00 1/1/2018,
     *                                              -----------------------
     *                                              SECTION @ Image,
     *                                              item2   @ 1:00 1/1/2018 ]
     */
    @Test
    public void testShowMenuButtonForImageSectionWithoutDate() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 2), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 1), OfflineItemFilter.FILTER_IMAGE);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(5, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        Assert.assertFalse(((SectionHeaderListItem) mModel.get(0)).showMenu);

        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 2), item1);
        assertSeparator(mModel.get(2), buildCalendar(2018, 1, 1, 0), false);
        assertSectionHeader(
                mModel.get(3), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_IMAGE, false);
        Assert.assertTrue(((SectionHeaderListItem) mModel.get(3)).showMenu);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 1, 1), item2);
    }

    /**
     * Action                                     List
     * 1. Set(item1 @ 2:00 1/1/2018 Image,        [ DATE    @ 0:00 1/1/2018,
     *        item2 @ 1:00 1/1/2018 Page)           SECTION @ Image,
     *                                              item1   @ 2:00 1/1/2018,
     *                                              -----------------------
     *                                              SECTION @ Page,
     *                                              item2   @ 1:00 1/1/2018 ]
     */
    @Test
    public void testShowMenuButtonForImageSectionWithDate() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 2), OfflineItemFilter.FILTER_IMAGE);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 1), OfflineItemFilter.FILTER_PAGE);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(5, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_IMAGE, true);
        Assert.assertTrue(((SectionHeaderListItem) mModel.get(0)).showMenu);

        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 2), item1);
        assertSeparator(mModel.get(2), buildCalendar(2018, 1, 1, 0), false);
        assertSectionHeader(
                mModel.get(3), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_PAGE, false);
        Assert.assertFalse(((SectionHeaderListItem) mModel.get(3)).showMenu);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 1, 1), item2);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 0:00 1/2/2018,        [ DATE    @ 0:00 1/2/2018,
     *        item2 @ 0:00 1/1/2018)          SECTION @ Video,
     *                                        item1   @ 0:00 1/2/2018,
     *                                        -----------------------
     *                                        DATE  @ 0:00 1/1/2018,
     *                                        SECTION @ Audio,
     *                                        item2   @ 0:00 1/1/2018 ]
     */
    @Test
    public void testTwoItemsDifferentDayMatchHeader() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_AUDIO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(5, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 0), item1);
        assertSeparator(mModel.get(2), buildCalendar(2018, 1, 2, 0), true);
        assertSectionHeader(
                mModel.get(3), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_AUDIO, true);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 1, 0), item2);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 4:00 1/1/2018,        [ DATE    @ 0:00 1/1/2018,
     *        item2 @ 5:00 1/1/2018)          SECTION @ Video,
     *                                        item2   @ 5:00 1/1/2018,
     *                                        item1   @ 4:00 1/1/2018 ]
     */
    @Test
    public void testTwoItemsSameDayOutOfOrder() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 5), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(3, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 5), item2);
        assertOfflineItem(mModel.get(2), buildCalendar(2018, 1, 1, 4), item1);
    }

    /**
     * Action                                      List
     * 1. Set(item1 @ 4:00 1/2/2018 Video,         [ DATE      @ 0:00 1/2/2018,
     *        item2 @ 5:00 1/1/2018 Video)           SECTION   @ Video,
     *                                               item2     @ 4:00 1/2/2018,
     *                                               -----------------------
     *                                               DATE      @ 0:00 1/1/2018,
     *                                               SECTION   @ Video,
     *                                               item1     @ 5:00 1/1/2018 ]
     */
    @Test
    public void testTwoItemsDifferentDaySameSection() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 4), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 5), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(5, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 4), item1);
        assertSeparator(mModel.get(2), buildCalendar(2018, 1, 2, 0), true);
        assertSectionHeader(
                mModel.get(3), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 1, 5), item2);
    }

    /**
     * Action                                      List
     * 1. Set(item1 @ 4:00 1/2/2018 Video,         [ DATE      @ 0:00 1/2/2018,
     *        item2 @ 5:00 1/1/2018 Page )           SECTION   @ Video,
     *                                               item2     @ 4:00 1/2/2018,
     *                                               -----------------------
     *                                               DATE      @ 0:00 1/1/2018,
     *                                               SECTION   @ Page,
     *                                               item1     @ 5:00 1/1/2018 ]
     */
    @Test
    public void testTwoItemsDifferentDayDifferentSection() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 4), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 5), OfflineItemFilter.FILTER_PAGE);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(5, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 4), item1);
        assertSeparator(mModel.get(2), buildCalendar(2018, 1, 2, 0), true);
        assertSectionHeader(
                mModel.get(3), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_PAGE, true);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 1, 5), item2);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 4:00 1/1/2018,        [ DATE   @ 0:00 1/2/2018,
     *        item2 @ 3:00 1/2/2018)          item2  @ 3:00 1/2/2018,
     *                                        -----------------------
     *                                        DATE   @ 0:00 1/1/2018,
     *                                        item1  @ 4:00 1/1/2018 ]
     */
    @Test
    public void testTwoItemsDifferentDayOutOfOrder() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 2, 3), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);

        Assert.assertEquals(5, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 3), item2);
        assertSeparator(mModel.get(2), buildCalendar(2018, 1, 2, 0), true);
        assertSectionHeader(
                mModel.get(3), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 1, 4), item1);
    }

    /**
     * Action                               List
     * 1. Set()                             [ ]
     *
     * 2. Add(item1 @ 4:00 1/1/2018)        [ DATE    @ 0:00 1/1/2018,
     *                                        item1  @ 4:00 1/1/2018 ]
     */
    @Test
    public void testAddItemToEmptyList() {
        when(mSource.getItems()).thenReturn(Collections.emptySet());
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        list.onItemsAdded(CollectionUtil.newArrayList(item1));

        Assert.assertEquals(2, mModel.size());
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 4), item1);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 1:00 1/2/2018)        [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 1:00 1/2/2018 ]
     * 2. Add(item2 @ 2:00 1/2/2018)        [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item2  @ 2:00 1/2/2018
     *                                        item1  @ 1:00 1/2/2018 ]
     * 3. Add(item3 @ 2:00 1/3/2018)        [ DATE    @ 0:00 1/3/2018,
     *                                        SECTION @ Video,
     *                                        item3  @ 2:00 1/3/2018
     *                                        -----------------------
     *                                        DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item2  @ 2:00 1/2/2018
     *                                        item1  @ 1:00 1/2/2018 ]
     */
    @Test
    public void testAddFirstItemToList() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 1), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        // Add an item on the same day that will be placed first.
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 2, 2), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        list.onItemsAdded(CollectionUtil.newArrayList(item2));

        Assert.assertEquals(3, mModel.size());
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 2), item2);
        assertOfflineItem(mModel.get(2), buildCalendar(2018, 1, 2, 1), item1);

        // Add an item on an earlier day that will be placed first.
        OfflineItem item3 =
                buildItem("3", buildCalendar(2018, 1, 3, 2), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2, item3));
        list.onItemsAdded(CollectionUtil.newArrayList(item3));

        Assert.assertEquals(6, mModel.size());
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 3, 2), item3);
        assertSeparator(mModel.get(2), buildCalendar(2018, 1, 3, 0), true);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 2, 2), item2);
        assertOfflineItem(mModel.get(5), buildCalendar(2018, 1, 2, 1), item1);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 4:00 1/2/2018)        [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 4:00 1/2/2018 ]
     *
     * 2. Add(item2 @ 3:00 1/2/2018)        [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 4:00 1/2/2018
     *                                        item2  @ 3:00 1/2/2018 ]
     *
     * 3. Add(item3 @ 4:00 1/1/2018)        [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 4:00 1/2/2018
     *                                        item2  @ 3:00 1/2/2018,
     *                                        -----------------------
     *                                        DATE    @ 0:00 1/1/2018,
     *                                        SECTION @ Video,
     *                                        item3  @ 4:00 1/1/2018
     */
    @Test
    public void testAddLastItemToList() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 4), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        // Add an item on the same day that will be placed last.
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 2, 3), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        list.onItemsAdded(CollectionUtil.newArrayList(item2));

        Assert.assertEquals(3, mModel.size());
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 4), item1);
        assertOfflineItem(mModel.get(2), buildCalendar(2018, 1, 2, 3), item2);

        // Add an item on a later day that will be placed last.
        OfflineItem item3 =
                buildItem("3", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2, item3));
        list.onItemsAdded(CollectionUtil.newArrayList(item3));

        Assert.assertEquals(6, mModel.size());
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 4), item1);
        assertOfflineItem(mModel.get(2), buildCalendar(2018, 1, 2, 3), item2);
        assertSeparator(mModel.get(3), buildCalendar(2018, 1, 2, 0), true);
        assertOfflineItem(mModel.get(5), buildCalendar(2018, 1, 1, 4), item3);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 2:00 1/2/2018)        [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 2:00 1/2/2018 ]
     *
     * 2. Remove(item1)                     [ ]
     */
    @Test
    public void testRemoveOnlyItemInList() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 2), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        when(mSource.getItems()).thenReturn(Collections.emptySet());
        list.onItemsRemoved(CollectionUtil.newArrayList(item1));

        Assert.assertEquals(0, mModel.size());
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 3:00 1/2/2018,        [ DATE    @ 0:00 1/2/2018,
     *        item2 @ 2:00 1/2/2018)          SECTION @ Video,
     *                                        item1  @ 3:00 1/2/2018,
     *                                        item2  @ 2:00 1/2/2018 ]
     *
     * 2. Remove(item1)                     [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item2  @ 2:00 1/2/2018 ]
     */
    @Test
    public void testRemoveFirstItemInListSameDay() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 3), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 2, 2), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item2));
        list.onItemsRemoved(CollectionUtil.newArrayList(item1));

        Assert.assertEquals(2, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 2), item2);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 3:00 1/2/2018,        [ DATE    @ 0:00 1/2/2018,
     *        item2 @ 2:00 1/2/2018)          SECTION @ Video,
     *                                        item1  @ 3:00 1/2/2018,
     *                                        item2  @ 2:00 1/2/2018 ]
     *
     * 2. Remove(item2)                     [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 3:00 1/2/2018 ]
     */
    @Test
    public void testRemoveLastItemInListSameDay() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 3), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 2, 2), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        list.onItemsRemoved(CollectionUtil.newArrayList(item2));

        Assert.assertEquals(2, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 3), item1);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 3:00 1/2/2018 Video,  [ DATE    @ 0:00 1/2/2018,
     *        item2 @ 2:00 1/2/2018 Image)    SECTION @ Video,
     *                                        item1  @ 3:00 1/2/2018,
     *                                        ----------------------
     *                                        SECTION @ Image,
     *                                        item2  @ 2:00 1/2/2018 ]
     *
     * 2. Remove(item1)                     [ DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Image,
     *                                        item2  @ 2:00 1/2/2018 ]
     */
    @Test
    public void testRemoveOnlyItemInSection() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 2, 3), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 2, 2), OfflineItemFilter.FILTER_IMAGE);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);
        Assert.assertEquals(5, mModel.size());

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item2));
        list.onItemsRemoved(CollectionUtil.newArrayList(item1));

        Assert.assertEquals(2, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_IMAGE, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 2), item2);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 3:00 1/3/2018,        [ DATE    @ 0:00 1/3/2018,
     *        item2 @ 2:00 1/2/2018)          SECTION @ Video,
     *                                        item1  @ 3:00 1/3/2018,
     *                                        -----------------------
     *                                        DATE    @ 0:00 1/2/2018,
     *                                        SECTION @ Video,
     *                                        item2  @ 2:00 1/2/2018 ]
     *
     * 2. Remove(item2)                     [ DATE    @ 0:00 1/3/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 3:00 1/3/2018 ]
     */
    @Test
    public void testRemoveLastItemInListWithMultipleDays() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 3, 3), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 2, 2), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        list.onItemsRemoved(CollectionUtil.newArrayList(item2));

        Assert.assertEquals(2, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 3, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 3, 3), item1);
    }

    /**
     * Action                               List
     * 1. Set()                             [ ]
     *
     * 2. Add(item1 @ 6:00  1/1/2018,       [ DATE    @ 0:00  1/2/2018,
     *        item2 @ 4:00  1/1/2018,         SECTION @ Video,
     *        item3 @ 10:00 1/2/2018,         item4  @ 12:00 1/2/2018,
     *        item4 @ 12:00 1/2/2018)         item3  @ 10:00 1/2/2018
     *                                        -----------------------
     *                                        DATE    @ 0:00  1/1/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 6:00  1/1/2018,
     *                                        item2  @ 4:00  1/1/2018 ]
     */
    @Test
    public void testAddMultipleItems() {
        when(mSource.getItems()).thenReturn(Collections.emptySet());
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 6), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item3 =
                buildItem("3", buildCalendar(2018, 1, 2, 10), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item4 =
                buildItem("4", buildCalendar(2018, 1, 2, 12), OfflineItemFilter.FILTER_VIDEO);

        when(mSource.getItems())
                .thenReturn(CollectionUtil.newArrayList(item1, item2, item3, item4));
        list.onItemsAdded(CollectionUtil.newArrayList(item1, item2, item3, item4));

        Assert.assertEquals(7, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 12), item4);
        assertOfflineItem(mModel.get(2), buildCalendar(2018, 1, 2, 10), item3);
        assertSeparator(mModel.get(3), buildCalendar(2018, 1, 2, 0), true);
        assertSectionHeader(
                mModel.get(4), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(5), buildCalendar(2018, 1, 1, 6), item1);
        assertOfflineItem(mModel.get(6), buildCalendar(2018, 1, 1, 4), item2);
    }

    /**
     * Action                               List
     * 2. Set(item1 @ 6:00  1/1/2018,       [ DATE    @ 0:00  1/2/2018,
     *        item2 @ 4:00  1/1/2018,         SECTION @ Video,
     *        item3 @ 10:00 1/2/2018,         item4  @ 12:00 1/2/2018,
     *        item4 @ 12:00 1/2/2018)         item3  @ 10:00 1/2/2018
     *                                        -----------------------
     *                                        DATE    @ 0:00  1/1/2018,
     *                                        SECTION @ Video,
     *                                        item1  @ 6:00  1/1/2018,
     *                                        item2  @ 4:00  1/1/2018 ]
     *
     * 2. Remove(item2,                     [ DATE    @ 0:00  1/1/2018,
     *           item3,                       SECTION @ Video,
     *           item4)                       item1  @ 6:00  1/1/2018 ]
     */
    @Test
    public void testRemoveMultipleItems() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 6), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item3 =
                buildItem("3", buildCalendar(2018, 1, 2, 10), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item4 =
                buildItem("4", buildCalendar(2018, 1, 2, 12), OfflineItemFilter.FILTER_VIDEO);

        when(mSource.getItems())
                .thenReturn(CollectionUtil.newArrayList(item1, item2, item3, item4));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        list.onItemsRemoved(CollectionUtil.newArrayList(item2, item3, item4));

        Assert.assertEquals(2, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 6), item1);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 4:00 1/1/2018)        [ DATE      @ 0:00  1/1/2018,
     *                                        SECTION @ Video,
     *                                        item1     @ 4:00  1/1/2018 ]
     *
     * 2. Update (item1,
     *            newItem1 @ 4:00 1/1/2018) [ DATE      @ 0:00  1/1/2018,
     *                                        SECTION @ Video,
     *                                        newItem1  @ 4:00  1/1/2018 ]
     */
    @Test
    public void testItemUpdatedSameTimestamp() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        // Update an item with the same timestamp.
        OfflineItem newItem1 =
                buildItem("1", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(newItem1));
        list.onItemUpdated(item1, newItem1);

        Assert.assertEquals(2, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 4), newItem1);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 5:00 1/1/2018,        [ DATE      @ 0:00  1/1/2018,
     *        item2 @ 4:00 1/1/2018)          SECTION @ Video,
     *                                        item1     @ 5:00  1/1/2018,
     *                                        item2     @ 4:00  1/1/2018
     * 2. Update (item1,
     *            newItem1 @ 3:00 1/1/2018) [ DATE      @ 0:00  1/1/2018,
     *                                        SECTION @ Video,
     *                                        item2     @ 4:00  1/1/2018,
     *                                        newItem1  @ 3:00  1/1/2018 ]
     */
    @Test
    public void testItemUpdatedSameDay() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 5), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        // Update an item with the same timestamp.
        OfflineItem newItem1 =
                buildItem("1", buildCalendar(2018, 1, 1, 3), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(newItem1, item2));
        list.onItemUpdated(item1, newItem1);

        Assert.assertEquals(3, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 4), item2);
        assertOfflineItem(mModel.get(2), buildCalendar(2018, 1, 1, 3), newItem1);
    }

    /**
     * Action                                   List
     * 1. Set(item1 @ 5:00 1/1/2018,            [ DATE      @ 0:00  1/1/2018,
     *        item2 @ 4:00 1/1/2018)              SECTION @ Video,
     *                                            item1     @ 5:00  1/1/2018,
     *                                            item2     @ 4:00  1/1/2018
     * 2. Update (item1,
     *            newItem1 @ 3:00 1/1/2018 Image) [ DATE      @ 0:00  1/1/2018,
     *                                              SECTION @ Video,
     *                                              item2     @ 4:00  1/1/2018,
     *                                              -------------------------
     *                                              SECTION @ Image,
     *                                              newItem1  @ 3:00  1/1/2018 ]
     */
    @Test
    public void testItemUpdatedSameDayDifferentSection() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 5), OfflineItemFilter.FILTER_VIDEO);
        OfflineItem item2 =
                buildItem("2", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1, item2));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        // Update an item with the same timestamp.
        OfflineItem newItem1 =
                buildItem("1", buildCalendar(2018, 1, 1, 3), OfflineItemFilter.FILTER_IMAGE);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(newItem1, item2));
        list.onItemUpdated(item1, newItem1);

        Assert.assertEquals(5, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 1, 4), item2);
        assertSectionHeader(
                mModel.get(3), buildCalendar(2018, 1, 1, 0), OfflineItemFilter.FILTER_IMAGE, false);
        assertOfflineItem(mModel.get(4), buildCalendar(2018, 1, 1, 3), newItem1);
    }

    /**
     * Action                               List
     * 1. Set(item1 @ 4:00 1/1/2018)        [ DATE      @ 0:00  1/1/2018,
     *                                        SECTION @ Video,
     *                                        item1     @ 4:00  1/1/2018 ]
     *
     * 2. Update (item1,
     *            newItem1 @ 6:00 1/2/2018) [ DATE      @ 0:00  1/2/2018,
     *                                        SECTION @ Video,
     *                                        newItem1  @ 6:00  1/2/2018 ]
     */
    @Test
    public void testItemUpdatedDifferentDay() {
        OfflineItem item1 =
                buildItem("1", buildCalendar(2018, 1, 1, 4), OfflineItemFilter.FILTER_VIDEO);

        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(item1));
        DateOrderedListMutator list = new DateOrderedListMutator(mSource, mModel);
        mModel.addObserver(mObserver);

        // Update an item with the same timestamp.
        OfflineItem newItem1 =
                buildItem("1", buildCalendar(2018, 1, 2, 6), OfflineItemFilter.FILTER_VIDEO);
        when(mSource.getItems()).thenReturn(CollectionUtil.newArrayList(newItem1));
        list.onItemUpdated(item1, newItem1);

        Assert.assertEquals(2, mModel.size());
        assertSectionHeader(
                mModel.get(0), buildCalendar(2018, 1, 2, 0), OfflineItemFilter.FILTER_VIDEO, true);
        assertOfflineItem(mModel.get(1), buildCalendar(2018, 1, 2, 6), newItem1);
    }

    private static Calendar buildCalendar(int year, int month, int dayOfMonth, int hourOfDay) {
        Calendar calendar = CalendarFactory.get();
        calendar.set(year, month, dayOfMonth, hourOfDay, 0);
        return calendar;
    }

    private static OfflineItem buildItem(
            String id, Calendar calendar, @OfflineItemFilter int filter) {
        OfflineItem item = new OfflineItem();
        item.id.namespace = "test";
        item.id.id = id;
        item.creationTimeMs = calendar.getTimeInMillis();
        item.filter = filter;
        return item;
    }

    private static void assertDatesAreEqual(Date date, Calendar calendar) {
        Calendar calendar2 = CalendarFactory.get();
        calendar2.setTime(date);
        Assert.assertEquals(calendar.getTimeInMillis(), calendar2.getTimeInMillis());
    }

    private static void assertOfflineItem(
            ListItem item, Calendar calendar, OfflineItem offlineItem) {
        Assert.assertTrue(item instanceof OfflineItemListItem);
        assertDatesAreEqual(((OfflineItemListItem) item).date, calendar);
        Assert.assertEquals(OfflineItemListItem.generateStableId(offlineItem), item.stableId);
        Assert.assertEquals(offlineItem, ((OfflineItemListItem) item).item);
    }

    private static void assertSectionHeader(
            ListItem item, Calendar calendar, @OfflineItemFilter int filter, boolean showDate) {
        Assert.assertTrue(item instanceof SectionHeaderListItem);
        SectionHeaderListItem sectionHeader = (SectionHeaderListItem) item;
        assertDatesAreEqual(sectionHeader.date, calendar);
        Assert.assertEquals(filter, sectionHeader.filter);
        Assert.assertEquals(
                SectionHeaderListItem.generateStableId(calendar.getTimeInMillis(), filter),
                item.stableId);
        Assert.assertEquals(sectionHeader.showDate, showDate);
    }

    private static void assertSeparator(ListItem item, Calendar calendar, boolean isDateDivider) {
        Assert.assertTrue(item instanceof SeparatorViewListItem);
        assertDatesAreEqual(((SeparatorViewListItem) item).date, calendar);
        Assert.assertEquals(isDateDivider, ((SeparatorViewListItem) item).isDateDivider());
    }
}
