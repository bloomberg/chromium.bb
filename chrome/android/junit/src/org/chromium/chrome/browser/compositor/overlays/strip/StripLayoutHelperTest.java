// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyFloat;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.util.DisplayMetrics;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.base.LocalizationUtils;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link StripLayoutHelper}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StripLayoutHelperTest {

    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    private TestTabModel mModel = new TestTabModel();
    private StripLayoutHelper mStripLayoutHelper;
    private static final String[] TEST_TAB_TITLES = {"Tab 1", "Tab 2", "Tab 3"};
    private static final String CLOSE_TAB = "Close tab test string";
    private static final String NEW_TAB = "New tab test string";

    /** Reset the environment before each test. */
    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getString(R.string.accessibility_tabstrip_btn_close_tab)).thenReturn(
                CLOSE_TAB);
        when(mResources.getString(R.string.accessibility_toolbar_btn_new_tab)).thenReturn(NEW_TAB);

        // CompositorButton
        when(mResources.getDisplayMetrics()).thenReturn(new DisplayMetrics());
        when(mResources.getDimension(anyInt())).thenReturn(100.0f);

        // ListPopupWindow
        final TypedArray mockTypedArray = mock(TypedArray.class);
        when(mockTypedArray.getDimension(anyInt(), anyFloat())).thenReturn(0f);
        when(mockTypedArray.getBoolean(anyInt(), anyBoolean())).thenReturn(false);
        when(mContext.obtainStyledAttributes(any(AttributeSet.class), any(int[].class), anyInt(),
                anyInt())).thenReturn(mockTypedArray);
        final Configuration mockConfiguration = mock(Configuration.class);
        when(mResources.getConfiguration()).thenReturn(mockConfiguration);
    }

    /**
     * Test method for {@link stripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * Checks that it returns the correct order of tabs, including correct content.
     */
    @Test
    @Feature({"Accessibility"})
    public void testSimpleTabOrder() {
        initializeTest(false, 0);

        assertTabStripAndOrder(TEST_TAB_TITLES);
    }

    /**
     * Test method for {@link stripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * Checks that it returns the correct order of tabs, even when a tab except the first one is
     * selected.
     */
    @Test
    @Feature({"Accessibility"})
    public void testTabOrderWithIndex() {
        initializeTest(false, 1);

        // Tabs should be in left to right order regardless of index
        assertTabStripAndOrder(TEST_TAB_TITLES);
    }

    /**
     * Test method for {@link stripLayoutHelper#getVirtualViews(List<VirtualView>)}.
     *
     * Checks that it returns the correct order of tabs, even in RTL mode.
     */
    @Test
    @Feature({"Accessibility"})
    public void testTabOrderRtl() {
        initializeTest(true, 0);

        // Tabs should be in linear order even in RTL.
        // Android will take care of reversing it.
        assertTabStripAndOrder(TEST_TAB_TITLES);
    }

    private void initializeTest(boolean rtl, int tabIndex) {
        mStripLayoutHelper = createStripLayoutHelper(rtl);
        for (int i = 0; i < TEST_TAB_TITLES.length; i++) {
            mModel.addTab(TEST_TAB_TITLES[i]);
        }
        mModel.setIndex(tabIndex);
        mStripLayoutHelper.setTabModel(mModel, null);
        mStripLayoutHelper.tabSelected(0, tabIndex, 0);
        // Flush UI updated
    }

    private void assertTabStripAndOrder(String[] expectedTabTitles) {
        // Each tab has a "close button", and there is one additional "new tab" button
        final int expectedNumberOfViews = 2 * expectedTabTitles.length + 1;

        final List<VirtualView> views = new ArrayList<VirtualView>();
        mStripLayoutHelper.getVirtualViews(views);
        assertEquals(expectedNumberOfViews, views.size());

        // Tab titles
        for (int i = 0; i < expectedTabTitles.length - 1; i++) {
            final String expectedTitle = i % 2 == 0
                    ? expectedTabTitles[i / 2] : CLOSE_TAB;
            assertEquals(expectedTitle, views.get(i).getAccessibilityDescription());
        }

        assertEquals(NEW_TAB, views.get(views.size() - 1).getAccessibilityDescription());
    }

    private StripLayoutHelper createStripLayoutHelper(boolean rtl) {
        LocalizationUtils.setRtlForTesting(rtl);
        final StripLayoutHelper stripLayoutHelper =
                new StripLayoutHelper(mContext, mUpdateHost, mRenderHost, false);
        // Initialize StackScroller
        stripLayoutHelper.onContextChanged(mContext);
        return stripLayoutHelper;
    }

    private class TestTabModel extends EmptyTabModel {
        private List<Tab> mMockTabs = new ArrayList<Tab>();
        private int mMaxId = -1;
        private int mIndex = 0;

        public void addTab(final String title) {
            mMaxId++;
            final Tab mockTab = mock(Tab.class);
            final int tabId = mMaxId;
            when(mockTab.getId()).thenReturn(tabId);
            when(mockTab.getTitle()).thenReturn(title);
            mMockTabs.add(mockTab);
        }

        @Override
        public Tab getTabAt(int id) {
            return mMockTabs.get(id);
        }

        @Override
        public int getCount() {
            return mMockTabs.size();
        }

        @Override
        public int index() {
            return mIndex;
        }

        public void setIndex(int index) {
            mIndex = index;
        }
    }
}