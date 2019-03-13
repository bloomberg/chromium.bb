// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.MatcherAssert.assertThat;

import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivity;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link TabListRecyclerView} and {@link TabGridContainerViewBinder}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGridContainerViewBinderTest extends DummyUiActivityTestCase {
    private static final int CONTAINER_HEIGHT = 56;
    private TabGridContainerViewBinder mTabGridContainerViewHolder;
    private PropertyModel mContainerModel;
    private PropertyModelChangeProcessor mMCP;
    private TabListRecyclerView mRecyclerView;
    private CallbackHelper mStartedShowingCallback;
    private CallbackHelper mFinishedShowingCallback;
    private CallbackHelper mStartedHidingCallback;
    private CallbackHelper mFinishedHidingCallback;
    private boolean mIsAnimating;

    private TabListRecyclerView.VisibilityListener mMockVisibilityListener =
            new TabListRecyclerView.VisibilityListener() {
                @Override
                public void startedShowing(boolean isAnimating) {
                    mStartedShowingCallback.notifyCalled();
                    mIsAnimating = isAnimating;
                }

                @Override
                public void finishedShowing() {
                    mFinishedShowingCallback.notifyCalled();
                }

                @Override
                public void startedHiding(boolean isAnimating) {
                    mStartedHidingCallback.notifyCalled();
                    mIsAnimating = isAnimating;
                }

                @Override
                public void finishedHiding() {
                    mFinishedHidingCallback.notifyCalled();
                }
            };

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        DummyUiActivity.setTestLayout(R.layout.tab_list_recycler_view_layout);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> { mRecyclerView = getActivity().findViewById(R.id.tab_list_view); });

        mStartedShowingCallback = new CallbackHelper();
        mFinishedShowingCallback = new CallbackHelper();
        mStartedHidingCallback = new CallbackHelper();
        mFinishedHidingCallback = new CallbackHelper();

        mContainerModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

        mMCP = PropertyModelChangeProcessor.create(
                mContainerModel, mRecyclerView, TabGridContainerViewBinder::bind);
    }

    @Test
    @MediumTest
    public void testShowWithAnimation() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(
                    TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, true);
            mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);
        });

        assertThat(mStartedShowingCallback.getCallCount(), equalTo(1));
        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        // TODO(yusufo): Find a way to test this on KitKat.
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) {
            assertThat(mRecyclerView.getAlpha(), equalTo(0.0f));
        }
        assertThat(mIsAnimating, equalTo(true));

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mRecyclerView.getAlpha() == 1.0f;
            }
        });
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testShowWithoutAnimation() throws Exception {
        mContainerModel.set(
                TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

        mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);

        assertThat(mStartedShowingCallback.getCallCount(), equalTo(1));
        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        assertThat(mRecyclerView.isAnimating(), equalTo(false));
        assertThat(mRecyclerView.getAlpha(), equalTo(1.0f));
        assertThat(mFinishedShowingCallback.getCallCount(), equalTo(1));
    }

    @Test
    @MediumTest
    public void testHidesWithAnimation() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(
                    TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
            mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);
        });

        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        assertThat(mRecyclerView.getAlpha(), equalTo(1.0f));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, true);
            mContainerModel.set(TabListContainerProperties.IS_VISIBLE, false);
        });

        assertThat(mStartedHidingCallback.getCallCount(), equalTo(1));
        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        // TODO(yusufo): Find a way to test this on KitKat.
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT) {
            assertThat(mRecyclerView.getAlpha(), equalTo(1.0f));
        }
        assertThat(mIsAnimating, equalTo(true));

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mRecyclerView.getAlpha() == 0.0f;
            }
        });
        assertThat(mRecyclerView.getVisibility(), equalTo(View.INVISIBLE));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testHidesWithoutAnimation() throws Exception {
        mContainerModel.set(
                TabListContainerProperties.VISIBILITY_LISTENER, mMockVisibilityListener);

        mContainerModel.set(TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES, false);
        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, true);

        assertThat(mRecyclerView.getVisibility(), equalTo(View.VISIBLE));
        assertThat(mRecyclerView.getAlpha(), equalTo(1.0f));

        mContainerModel.set(TabListContainerProperties.IS_VISIBLE, false);

        assertThat(mStartedHidingCallback.getCallCount(), equalTo(1));
        assertThat(mRecyclerView.isAnimating(), equalTo(false));
        assertThat(mRecyclerView.getAlpha(), equalTo(0.0f));
        assertThat(mRecyclerView.getVisibility(), equalTo(View.INVISIBLE));
        assertThat(mFinishedHidingCallback.getCallCount(), equalTo(1));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testIsIncognitoSetsBackgroundColor() throws Exception {
        mContainerModel.set(TabListContainerProperties.IS_INCOGNITO, true);
        assertThat(mRecyclerView.getBackground(), instanceOf(ColorDrawable.class));
        assertThat(((ColorDrawable) mRecyclerView.getBackground()).getColor(),
                equalTo(ApiCompatibilityUtils.getColor(
                        mRecyclerView.getResources(), R.color.incognito_modern_primary_color)));

        mContainerModel.set(TabListContainerProperties.IS_INCOGNITO, false);
        assertThat(mRecyclerView.getBackground(), instanceOf(ColorDrawable.class));
        assertThat(((ColorDrawable) mRecyclerView.getBackground()).getColor(),
                equalTo(ApiCompatibilityUtils.getColor(
                        mRecyclerView.getResources(), R.color.modern_primary_color)));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testTopContainerHeightSetsTopMargin() throws Exception {
        assertThat(mRecyclerView.getLayoutParams(), instanceOf(FrameLayout.LayoutParams.class));
        assertThat(
                ((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin, equalTo(0));

        mContainerModel.set(TabListContainerProperties.TOP_CONTROLS_HEIGHT, CONTAINER_HEIGHT);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).topMargin,
                equalTo(CONTAINER_HEIGHT));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testBottomContainerHeightSetsBottomMargin() throws Exception {
        assertThat(mRecyclerView.getLayoutParams(), instanceOf(FrameLayout.LayoutParams.class));
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).bottomMargin,
                equalTo(0));

        mContainerModel.set(TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT, CONTAINER_HEIGHT);
        assertThat(((FrameLayout.LayoutParams) mRecyclerView.getLayoutParams()).bottomMargin,
                equalTo(CONTAINER_HEIGHT));
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }
}
