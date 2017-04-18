// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.RemoteViews;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the SearchWidgetProvider.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SearchWidgetProviderTest {
    private static final class TestDelegate
            extends SearchWidgetProvider.SearchWidgetProviderDelegate {
        public static final int[] ALL_IDS = {11684, 20170525};

        public final List<Pair<Integer, RemoteViews>> mViews = new ArrayList<>();

        private TestDelegate(Context context) {
            super(context);
        }

        @Override
        protected int[] getAllSearchWidgetIds() {
            return ALL_IDS;
        }

        @Override
        protected void updateAppWidget(int id, RemoteViews views) {
            mViews.add(new Pair<Integer, RemoteViews>(id, views));
        }
    }

    private static final class TestContext extends AdvancedMockContext {
        public TestContext() {
            // Wrapping the application context allows the ContextUtils to avoid writing to the real
            // SharedPreferences file.
            super(InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getApplicationContext());
        }
    }

    private static final String TEXT_GENERIC = "Search";
    private static final String TEXT_SEARCH_ENGINE = "Stuff and Thangs";
    private static final String TEXT_SEARCH_ENGINE_FULL = "Search with Stuff and Thangs";

    private TestContext mContext;
    private TestDelegate mDelegate;

    @Before
    public void setUp() throws Exception {
        SearchActivity.disableForTests();

        mContext = new TestContext();
        ContextUtils.initApplicationContextForTests(mContext);

        mDelegate = new TestDelegate(mContext);
        SearchWidgetProvider.setDelegateForTest(mDelegate);
    }

    @Test
    @SmallTest
    public void testUpdateAll() {
        SearchWidgetProvider.handleAction(SearchWidgetProvider.ACTION_UPDATE_ALL_WIDGETS);

        // Without any idea of what the default search engine is, widgets should default to saying
        // just "Search".
        checkWidgetStates(TEXT_GENERIC, View.VISIBLE);

        // The microphone icon should disappear if voice queries are unavailable.
        mDelegate.mViews.clear();
        SearchWidgetProvider.updateCachedVoiceSearchAvailability(false);
        checkWidgetStates(TEXT_GENERIC, View.GONE);

        // After recording that the default search engine is "X", it should say "Search with X".
        mDelegate.mViews.clear();
        SearchWidgetProvider.updateCachedEngineName(TEXT_SEARCH_ENGINE);
        checkWidgetStates(TEXT_SEARCH_ENGINE_FULL, View.GONE);

        // The microphone icon should appear if voice queries are available.
        mDelegate.mViews.clear();
        SearchWidgetProvider.updateCachedVoiceSearchAvailability(true);
        checkWidgetStates(TEXT_SEARCH_ENGINE_FULL, View.VISIBLE);
    }

    private void checkWidgetStates(final String expectedString, final int expectedMicrophoneState) {
        // Confirm that all the widgets got updated.
        Assert.assertEquals(TestDelegate.ALL_IDS.length, mDelegate.mViews.size());
        for (int i = 0; i < TestDelegate.ALL_IDS.length; i++) {
            Assert.assertEquals(TestDelegate.ALL_IDS[i], mDelegate.mViews.get(i).first.intValue());
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Check the contents of the RemoteViews by inflating them.
                for (int i = 0; i < mDelegate.mViews.size(); i++) {
                    FrameLayout parentView = new FrameLayout(mContext);
                    RemoteViews views = mDelegate.mViews.get(i).second;
                    View view = views.apply(mContext, parentView);
                    parentView.addView(view);

                    // Confirm that the string is correct.
                    TextView titleView = (TextView) view.findViewById(R.id.title);
                    Assert.assertEquals(View.VISIBLE, titleView.getVisibility());
                    Assert.assertEquals(expectedString, titleView.getText());

                    // Confirm the visibility of the microphone.
                    View microphoneView = view.findViewById(R.id.microphone_icon);
                    Assert.assertEquals(expectedMicrophoneState, microphoneView.getVisibility());
                }
            }
        });
    }

    @Test
    @SmallTest
    public void testMicrophoneClick() {
        SearchWidgetProvider.handleAction(SearchWidgetProvider.ACTION_UPDATE_ALL_WIDGETS);
        for (int i = 0; i < mDelegate.mViews.size(); i++) {
            RemoteViews views = mDelegate.mViews.get(i).second;
            clickOnWidget(views, R.id.microphone_icon);
        }
    }

    @Test
    @SmallTest
    public void testTextClick() {
        SearchWidgetProvider.handleAction(SearchWidgetProvider.ACTION_UPDATE_ALL_WIDGETS);
        for (int i = 0; i < mDelegate.mViews.size(); i++) {
            RemoteViews views = mDelegate.mViews.get(i).second;
            clickOnWidget(views, R.id.text_container);
        }
    }

    private void clickOnWidget(final RemoteViews views, final int clickTarget) {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final ActivityMonitor monitor =
                new ActivityMonitor(SearchActivity.class.getName(), null, false);
        instrumentation.addMonitor(monitor);

        // Click on the widget.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FrameLayout parentView = new FrameLayout(mContext);
                View view = views.apply(mContext, parentView);
                parentView.addView(view);
                view.findViewById(clickTarget).performClick();
            }
        });

        // Check that the Activity was launched in the right mode.
        Activity activity = instrumentation.waitForMonitor(monitor);
        Intent intent = activity.getIntent();
        boolean microphoneState = IntentUtils.safeGetBooleanExtra(
                intent, SearchWidgetProvider.EXTRA_START_VOICE_SEARCH, false);
        Assert.assertEquals(clickTarget == R.id.microphone_icon, microphoneState);
    }
}
