// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.GridLayoutManager;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Integration tests for TabGridMessageCardProvider component.
 */
@Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
public class MessageCardProviderTest extends DummyUiActivityTestCase {
    private static final int SUGGESTED_TAB_COUNT = 2;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private TabListRecyclerView mRecyclerView;
    private TabListRecyclerView.VisibilityListener mRecyclerViewVisibilityListener =
            new TabListRecyclerView.VisibilityListener() {
                @Override
                public void startedShowing(boolean isAnimating) {
                    List<MessageCardProviderMediator.Message> messageList =
                            mCoordinator.getMessageItems();
                    for (int i = 0; i < messageList.size(); i++) {
                        MessageCardProviderMediator.Message message = messageList.get(i);
                        mModelList.add(new MVCListAdapter.ListItem(
                                TabProperties.UiType.MESSAGE, message.model));
                    }
                }

                @Override
                public void finishedShowing() {
                    mFinishedShowing.set(true);
                }

                @Override
                public void startedHiding(boolean isAnimating) {
                    mFinishedShowing.set(false);
                    mModelList.clear();
                }

                @Override
                public void finishedHiding() {}
            };

    private TabListModel mModelList = new TabListModel();
    private SimpleRecyclerViewAdapter mAdapter;

    private AtomicBoolean mFinishedShowing = new AtomicBoolean(false);

    private MessageService mTestingService =
            new MessageService(MessageService.MessageType.FOR_TESTING);
    private MessageService mSuggestionService =
            new MessageService(MessageService.MessageType.TAB_SUGGESTION);
    private MessageCardProviderCoordinator mCoordinator;

    private MessageCardView.DismissActionProvider mUiDismissActionProvider = (messageType) -> {};

    @Mock
    private TabSuggestionMessageService.TabSuggestionMessageData mTabSuggestionMessageData;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
        // TODO(meiliang): Replace with TabSwitcher instead when ready to integrate with
        // TabSwitcher.
        ViewGroup view = new FrameLayout(getActivity());
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view);

            mRecyclerView = (TabListRecyclerView) getActivity().getLayoutInflater().inflate(
                    R.layout.tab_list_recycler_view_layout, view, false);
            mRecyclerView.setVisibilityListener(mRecyclerViewVisibilityListener);
            mRecyclerView.setVisibility(View.INVISIBLE);

            mAdapter.registerType(TabProperties.UiType.MESSAGE,
                    new LayoutViewBuilder(R.layout.tab_grid_message_card_item),
                    MessageCardViewBinder::bind);

            GridLayoutManager layoutManager = new GridLayoutManager(mRecyclerView.getContext(), 2);
            layoutManager.setSpanSizeLookup(new GridLayoutManager.SpanSizeLookup() {
                @Override
                public int getSpanSize(int i) {
                    int itemType = mAdapter.getItemViewType(i);

                    if (itemType == TabProperties.UiType.MESSAGE) return 2;
                    return 1;
                }
            });
            mRecyclerView.setLayoutManager(layoutManager);
            mRecyclerView.setAdapter(mAdapter);

            view.addView(mRecyclerView);
        });

        mCoordinator = new MessageCardProviderCoordinator(getActivity(), mUiDismissActionProvider);
        mCoordinator.subscribeMessageService(mTestingService);
        mCoordinator.subscribeMessageService(mSuggestionService);
    }

    @Test
    @SmallTest
    public void testShowingTabSuggestionMessage() {
        when(mTabSuggestionMessageData.getSize()).thenReturn(SUGGESTED_TAB_COUNT);
        mSuggestionService.sendAvailabilityNotification(mTabSuggestionMessageData);

        TestThreadUtils.runOnUiThreadBlocking(() -> mRecyclerView.startShowing(false));

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReviewTabSuggestionMessage() {
        AtomicBoolean reviewed = new AtomicBoolean();
        when(mTabSuggestionMessageData.getSize()).thenReturn(SUGGESTED_TAB_COUNT);
        when(mTabSuggestionMessageData.getReviewActionProvider())
                .thenReturn(() -> reviewed.set(true));
        mSuggestionService.sendAvailabilityNotification(mTabSuggestionMessageData);

        TestThreadUtils.runOnUiThreadBlocking(() -> mRecyclerView.startShowing(false));

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        assertFalse(reviewed.get());
        onView(withId(R.id.action_button)).perform(click());
        assertTrue(reviewed.get());
    }

    @Test
    @SmallTest
    public void testDismissTabSuggestionMessage() {
        AtomicBoolean dismissed = new AtomicBoolean();
        when(mTabSuggestionMessageData.getSize()).thenReturn(SUGGESTED_TAB_COUNT);
        when(mTabSuggestionMessageData.getDismissActionProvider())
                .thenReturn((type) -> dismissed.set(true));
        mSuggestionService.sendAvailabilityNotification(mTabSuggestionMessageData);

        TestThreadUtils.runOnUiThreadBlocking(() -> mRecyclerView.startShowing(false));

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        assertFalse(dismissed.get());
        onView(withId(R.id.close_button)).perform(click());
        assertTrue(dismissed.get());
    }
}
