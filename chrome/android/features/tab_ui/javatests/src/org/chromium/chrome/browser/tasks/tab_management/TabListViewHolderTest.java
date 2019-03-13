// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.MatcherAssert.assertThat;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for the {@link android.support.v7.widget.RecyclerView.ViewHolder} classes for {@link
 * TabListCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabListViewHolderTest extends DummyUiActivityTestCase {
    private TabGridViewHolder mTabGridViewHolder;
    private PropertyModel mGridModel;
    private PropertyModelChangeProcessor mGridMCP;

    private TabStripViewHolder mTabStripViewHolder;
    private PropertyModel mStripModel;
    private PropertyModelChangeProcessor mStripMCP;

    private TabListMediator.ThumbnailFetcher mMockThumbnailProvider =
            new TabListMediator.ThumbnailFetcher(new TabListMediator.ThumbnailProvider() {
                @Override
                public void getTabThumbnailWithCallback(Tab tab, Callback<Bitmap> callback) {
                    Bitmap bitmap = mShouldReturnBitmap
                            ? Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888)
                            : null;
                    callback.onResult(bitmap);
                }
            }, null);

    private TabListMediator.TabActionListener mMockCloseListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(int tabId) {
                    mCloseClicked.set(true);
                }
            };
    private AtomicBoolean mCloseClicked = new AtomicBoolean();

    private TabListMediator.TabActionListener mMockSelectedListener =
            new TabListMediator.TabActionListener() {
                @Override
                public void run(int tabId) {
                    mSelectClicked.set(true);
                }
            };
    private AtomicBoolean mSelectClicked = new AtomicBoolean();
    private boolean mShouldReturnBitmap;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        ViewGroup view = new LinearLayout(getActivity());
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view, params);

            mTabGridViewHolder = TabGridViewHolder.create(view, 0);
            mTabStripViewHolder = TabStripViewHolder.create(view, 0);

            view.addView(mTabGridViewHolder.itemView);
            view.addView(mTabStripViewHolder.itemView);
        });

        mGridModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                             .with(TabProperties.TAB_SELECTED_LISTENER, mMockSelectedListener)
                             .with(TabProperties.TAB_CLOSED_LISTENER, mMockCloseListener)
                             .build();
        mStripModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_STRIP)
                              .with(TabProperties.TAB_SELECTED_LISTENER, mMockSelectedListener)
                              .with(TabProperties.TAB_CLOSED_LISTENER, mMockCloseListener)
                              .build();

        mGridMCP = PropertyModelChangeProcessor.create(mGridModel, mTabGridViewHolder,
                new TestRecyclerViewSimpleViewBinder<>(TabGridViewBinder::onBindViewHolder));
        mStripMCP = PropertyModelChangeProcessor.create(mStripModel, mTabStripViewHolder,
                new TestRecyclerViewSimpleViewBinder<>(TabStripViewBinder::onBindViewHolder));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSelected() throws Exception {
        mGridModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertTrue(((FrameLayout) (mTabGridViewHolder.itemView)).getForeground() != null);
        mGridModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertFalse(((FrameLayout) (mTabGridViewHolder.itemView)).getForeground() != null);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        Assert.assertTrue(((FrameLayout) (mTabStripViewHolder.itemView)).getForeground() != null);
        mStripModel.set(TabProperties.IS_SELECTED, false);
        Assert.assertFalse(((FrameLayout) (mTabStripViewHolder.itemView)).getForeground() != null);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testTitle() throws Exception {
        final String title = "Surf the cool webz";
        mGridModel.set(TabProperties.TITLE, title);
        Assert.assertEquals(mTabGridViewHolder.title.getText(), title);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testThumbnail() throws Exception {
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        // This should have set the image resource id to 0 and reset it.
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) {
            Assert.assertNull(mTabGridViewHolder.thumbnail.getDrawable());
        } else {
            assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(ColorDrawable.class));
        }
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, null);

        mShouldReturnBitmap = true;
        mGridModel.set(TabProperties.THUMBNAIL_FETCHER, mMockThumbnailProvider);
        assertThat(mTabGridViewHolder.thumbnail.getDrawable(), instanceOf(BitmapDrawable.class));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickToSelect() throws Exception {
        mTabGridViewHolder.itemView.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, false);
        mTabStripViewHolder.button.performClick();
        Assert.assertTrue(mSelectClicked.get());
        mSelectClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        mTabStripViewHolder.button.performClick();
        Assert.assertFalse(mSelectClicked.get());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testClickToClose() throws Exception {
        mTabGridViewHolder.closeButton.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, true);
        mTabStripViewHolder.button.performClick();
        Assert.assertTrue(mCloseClicked.get());
        mCloseClicked.set(false);

        mStripModel.set(TabProperties.IS_SELECTED, false);
        mTabStripViewHolder.button.performClick();
        Assert.assertFalse(mCloseClicked.get());
    }

    @Override
    public void tearDownTest() throws Exception {
        mStripMCP.destroy();
        mGridMCP.destroy();
        super.tearDownTest();
    }
}
