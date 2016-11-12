// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;

import static android.support.test.espresso.Espresso.onData;
import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.actionWithAssertions;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.intent.Intents.assertNoUnverifiedIntents;
import static android.support.test.espresso.intent.Intents.intended;
import static android.support.test.espresso.intent.Intents.intending;
import static android.support.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static android.support.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasExtra;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasType;
import static android.support.test.espresso.matcher.RootMatchers.DEFAULT;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withClassName;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static android.support.test.espresso.web.assertion.WebViewAssertions.webMatches;
import static android.support.test.espresso.web.sugar.Web.onWebView;
import static android.support.test.espresso.web.webdriver.DriverAtoms.findElement;
import static android.support.test.espresso.web.webdriver.DriverAtoms.getText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.endsWith;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.core.AnyOf.anyOf;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.espresso.PerformException;
import android.support.test.espresso.Root;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.action.GeneralClickAction;
import android.support.test.espresso.action.GeneralLocation;
import android.support.test.espresso.action.Press;
import android.support.test.espresso.action.Tap;
import android.support.test.espresso.intent.Intents;
import android.support.test.espresso.web.webdriver.Locator;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiSelector;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MenuItem;

import junit.framework.AssertionFailedError;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.webview_ui_test.R;
import org.chromium.webview_ui_test.WebViewUiTestActivity;
import org.chromium.webview_ui_test.test.util.UseLayout;
import org.chromium.webview_ui_test.test.util.WebViewUiTestRule;

import java.lang.reflect.Method;

/**
 * Tests for WebView ActionMode.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ActionModeTest {
    private static final String TAG = "ActionModeTest";

    // Actions available in action mode
    private static final String ASSIST_ACTION = "Assist";
    private static final String COPY_ACTION = "Copy";
    private static final String MORE_OPTIONS_ACTION = "More options";
    private static final String PASTE_ACTION = "Paste";
    private static final String SHARE_ACTION = "Share";
    private static final String SELECT_ALL_ACTION = "Select All";
    private static final String WEB_SEARCH_ACTION = "Web search";

    private static final String QUICK_SEARCH_BOX_PKG = "com.google.android.googlequicksearchbox";
    private static final long ASSIST_TIMEOUT = scaleTimeout(5000);

    @Rule
    public WebViewUiTestRule mWebViewActivityRule =
            new WebViewUiTestRule(WebViewUiTestActivity.class);

    @Before
    public void setUp() {
        mWebViewActivityRule.launchActivity();
        onWebView().forceJavascriptEnabled();
        mWebViewActivityRule.loadDataSync(
                "<html><body><p>Hello world</p></body></html>", "text/html", "utf-8", false);
        onWebView(withId(R.id.webview))
                .withElement(findElement(Locator.TAG_NAME, "p"))
                .check(webMatches(getText(), containsString("Hello world")));
        disableAnimation();
    }

    /**
     * Only way to disable popup animations.
     */
    private void disableAnimation() {
        try {
            // This is a hidden method to disable animations.  It is also being used by CTS tests.
            Method setDurationScale = ValueAnimator.class.getMethod(
                    "setDurationScale", float.class);
            setDurationScale.invoke(null, 0.0f);
        } catch (Exception e) {
            Log.e(TAG, "Couldn't disable animation", e);
        }
    }

    /**
     * Test Copy and Paste
     */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testCopyPaste() {
        onView(withId(R.id.webview)).perform(longClickOnLastWord());
        clickPopupAction(COPY_ACTION);
        onView(withId(R.id.edittext)).perform(longClickOnLastWord());
        clickPopupAction(PASTE_ACTION);
        onView(withId(R.id.edittext))
                .check(matches(withText("world")));
    }

    /**
     * Test Select All
     */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testSelectAll() {
        onView(withId(R.id.webview)).perform(longClickOnLastWord());
        clickPopupAction("Select all");
        clickPopupAction(COPY_ACTION);
        onView(withId(R.id.edittext)).perform(longClickOnLastWord());
        clickPopupAction(PASTE_ACTION);
        onView(withId(R.id.edittext))
                .check(matches(withText("Hello world")));
    }

    /**
     * Test Share
     */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testShare() {
        Intents.init();
        intending(anyIntent())
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, new Intent()));

        onView(withId(R.id.webview)).perform(longClickOnLastWord());
        clickPopupAction(SHARE_ACTION);

        intended(allOf(hasAction(Intent.ACTION_CHOOSER),
                hasExtras(allOf(hasEntry(Intent.EXTRA_TITLE, SHARE_ACTION),
                        hasEntry(Intent.EXTRA_INTENT,
                                allOf(hasAction(Intent.ACTION_SEND), hasType("text/plain"),
                                        hasExtra(Intent.EXTRA_TEXT, "world")))))));
        assertNoUnverifiedIntents();
    }

    /**
     * Test Web Search
     */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testWebSearch() {
        Intents.init();
        intending(anyIntent())
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, new Intent()));
        onView(withId(R.id.webview)).perform(longClickOnLastWord());
        clickPopupAction(WEB_SEARCH_ACTION);
        intended(allOf(hasAction(Intent.ACTION_WEB_SEARCH),
                hasExtras(allOf(hasEntry("com.android.browser.application_id",
                                         "org.chromium.webview_ui_test"),
                                hasEntry("query", "world"),
                                hasEntry("new_search", true)))));
        assertNoUnverifiedIntents();
    }

    /**
     * Test Assist
     */
    @Test
    @SmallTest
    @UseLayout("edittext_webview")
    public void testAssist() {
        // TODO(aluo): Get SdkSuppress to work with the test runner
        if (Build.VERSION.SDK_INT < 24) return;
        onView(withId(R.id.webview)).perform(longClickOnLastWord());
        clickPopupAction(ASSIST_ACTION);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        UiObject assistUi = device.findObject(new UiSelector().packageName(QUICK_SEARCH_BOX_PKG));
        assertTrue(assistUi.waitForExists(ASSIST_TIMEOUT));
        device.pressBack();
    }

    /**
     * Click an item on the Action Mode popup
     */
    public void clickPopupAction(final String name) {
        Matcher<Root> rootMatcher;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            try {
                // On L and lower, use the espresso DEFAULT root matcher if ActionBar is detected
                onView(withClassName(endsWith("widget.ActionBarContextView")))
                        .check(matches(isDisplayed()));
                rootMatcher = DEFAULT;
            } catch (NoMatchingViewException | AssertionFailedError e) {
                // Else match in a popup
                rootMatcher = withDecorView(withChild(withText(name)));
            }
        } else {
            // On M and above, can use the decoreView matcher
            rootMatcher = withDecorView(isEnabled());
        }
        try {
            onView(anyOf(withText(name), withContentDescription(name)))
                    .inRoot(rootMatcher)
                    .perform(click());
        } catch (PerformException e) {
            // Take care of case when the item is in the overflow menu
            onView(withContentDescription(MORE_OPTIONS_ACTION))
                    .inRoot(withDecorView(isEnabled()))
                    .perform(click());
            onData(new MenuItemMatcher(equalTo(name))).inRoot(rootMatcher).perform(click());
        }
    }

    /**
     * This view action clicks on center right of a view to select the last word
     */
    private static final ViewAction longClickOnLastWord() {
        // TODO(aluo): This function is not guaranteed to click on element. Change to
        // implementation that gets bounding box for elements using Javascript.
        return actionWithAssertions(
                new GeneralClickAction(Tap.LONG, GeneralLocation.CENTER_RIGHT, Press.FINGER));
    }

    /**
     * Matches an item on the Action Mode popup by the title
     */
    private static class MenuItemMatcher extends TypeSafeMatcher<MenuItem> {
        private Matcher<String> mTitleMatcher;

        public MenuItemMatcher(Matcher<String> titleMatcher) {
            mTitleMatcher = titleMatcher;
        }

        @Override
        protected boolean matchesSafely(MenuItem item) {
            return mTitleMatcher.matches(item.getTitle());
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("has MenuItem with title: ");
            description.appendDescriptionOf(mTitleMatcher);
        }
    }
}
