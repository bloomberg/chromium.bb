// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.chrome.browser.suggestions.SuggestionsSheetVisibilityChangeObserverTest.TestVisibilityChangeObserver.Event.Hidden;
import static org.chromium.chrome.browser.suggestions.SuggestionsSheetVisibilityChangeObserverTest.TestVisibilityChangeObserver.Event.InitialReveal;
import static org.chromium.chrome.browser.suggestions.SuggestionsSheetVisibilityChangeObserverTest.TestVisibilityChangeObserver.Event.StateChange;
import static org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController.TYPE_SUGGESTIONS;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.action.ViewActions;
import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.NtpUiCaptureTestData;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link SuggestionsSheetVisibilityChangeObserver}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public class SuggestionsSheetVisibilityChangeObserverTest {
    @Rule
    public BottomSheetTestRule mActivityRule = new BottomSheetTestRule();

    @Rule
    public SuggestionsDependenciesRule createSuggestions() {
        mEventReporter = new SpyEventReporter();
        SuggestionsDependenciesRule.TestFactory depsFactory = NtpUiCaptureTestData.createFactory();
        depsFactory.eventReporter = mEventReporter;
        return new SuggestionsDependenciesRule(depsFactory);
    }

    private SpyEventReporter mEventReporter;
    private TestVisibilityChangeObserver mObserver;

    @Before
    public void setUp() throws InterruptedException {
        mActivityRule.startMainActivityOnBottomSheet(BottomSheet.SHEET_STATE_PEEK);
        // The home sheet should not be initialised.
        mEventReporter.surfaceOpenedHelper.verifyCallCount();

        // Register the change observer
        mObserver = new TestVisibilityChangeObserver(mActivityRule.getActivity());
        mObserver.expectEvents();
    }

    @Test
    @MediumTest
    public void testHomeSheetVisibilityOnWebPage() {
        // Pull sheet to half. We use the animated variants to be closer to user triggered events.
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, true);
        mObserver.expectEvents(InitialReveal, StateChange);
        mEventReporter.surfaceOpenedHelper.waitForCallback();

        // Pull sheet to full.
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_FULL, true);
        mObserver.expectEvents(StateChange);

        // close
        Espresso.pressBack();
        mObserver.expectEvents(Hidden, StateChange, StateChange);
    }

    @Test
    @MediumTest
    public void testHomeSheetVisibilityOnOmnibox() {
        // Tap the omnibox. The home sheet content should not be notified it is selected.
        Espresso.onView(ViewMatchers.withId(R.id.url_bar)).perform(ViewActions.click());
        waitForWindowUpdates();

        mObserver.expectEvents();
        assertEquals(BottomSheet.SHEET_STATE_FULL, mActivityRule.getBottomSheet().getSheetState());

        // Back closes the bottom sheet.
        Espresso.pressBack();
        waitForWindowUpdates();

        mObserver.expectEvents();
        assertEquals(BottomSheet.SHEET_STATE_PEEK, mActivityRule.getBottomSheet().getSheetState());

        mEventReporter.surfaceOpenedHelper.verifyCallCount();
    }

    @Test
    @MediumTest
    public void testHomeSheetVisibilityOnOmniboxAndSwipeToolbar() {
        // Tap the omnibox. The home sheet content should not be notified it is selected.
        Espresso.onView(ViewMatchers.withId(R.id.url_bar)).perform(ViewActions.click());
        waitForWindowUpdates();

        mObserver.expectEvents();
        assertEquals(BottomSheet.SHEET_STATE_FULL, mActivityRule.getBottomSheet().getSheetState());

        // Changing the state of the sheet closes the omnibox suggestions and shows the home sheet.
        mActivityRule.setSheetState(BottomSheet.SHEET_STATE_HALF, true);
        mObserver.expectEvents(InitialReveal, StateChange);
        mEventReporter.surfaceOpenedHelper.waitForCallback();

        // Back closes the bottom sheet.
        Espresso.pressBack();
        mObserver.expectEvents(Hidden, StateChange, StateChange);
        assertEquals(BottomSheet.SHEET_STATE_PEEK, mActivityRule.getBottomSheet().getSheetState());

        mEventReporter.surfaceOpenedHelper.verifyCallCount();
    }

    @Test
    @MediumTest
    public void testHomeSheetVisibilityOnNewTab() {
        // Show the new tab view
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityRule.getBottomSheet().getNewTabController().displayNewTabUi(false));
        mObserver.expectEvents(InitialReveal, StateChange);
        mEventReporter.surfaceOpenedHelper.waitForCallback();
        assertEquals(BottomSheet.SHEET_STATE_HALF, mActivityRule.getBottomSheet().getSheetState());

        // Tap the omnibox. The bottom sheet should expand to full, with the keyboard coming up.
        Espresso.onView(ViewMatchers.withId(R.id.url_bar)).perform(ViewActions.click());
        mObserver.expectEvents(StateChange);
        assertEquals(BottomSheet.SHEET_STATE_FULL, mActivityRule.getBottomSheet().getSheetState());

        // Type in the omnibox, the omnibox suggestion list should come hide the home sheet.
        Espresso.onView(ViewMatchers.withId(R.id.url_bar)).perform(ViewActions.typeText("g"));
        // TODO(https://crbug.com/731128): Known issue, we don't have events associated to the
        // overlay covering the home sheet content.
        // observer.expectEvents(Hidden, StateChange);
        assertEquals(BottomSheet.SHEET_STATE_FULL, mActivityRule.getBottomSheet().getSheetState());

        // Back hides the omnibox suggestions.
        Espresso.pressBack();
        waitForWindowUpdates();

        // TODO(https://crbug.com/731128): Same as previous events not being sent. But here it does
        // not matter too much since the critical interaction is onSurfaceOpened not being called
        // a second time.
        // observer.expectEvents(Shown, StateChange);
        assertEquals(BottomSheet.SHEET_STATE_FULL, mActivityRule.getBottomSheet().getSheetState());
        mEventReporter.surfaceOpenedHelper.verifyCallCount();

        // Back again closes the bottom sheet
        Espresso.pressBack();
        mObserver.expectEvents(Hidden, StateChange, StateChange);
        assertEquals(BottomSheet.SHEET_STATE_PEEK, mActivityRule.getBottomSheet().getSheetState());

        mEventReporter.surfaceOpenedHelper.verifyCallCount();
    }

    static class TestVisibilityChangeObserver extends SuggestionsSheetVisibilityChangeObserver {
        public enum Event { InitialReveal, Shown, Hidden, StateChange }

        private final ChromeActivity mActivity;

        private SelfVerifyingCallbackHelper mInitialRevealHelper =
                new SelfVerifyingCallbackHelper("InitialReveal");
        private SelfVerifyingCallbackHelper mShownHelper = new SelfVerifyingCallbackHelper("Shown");
        private SelfVerifyingCallbackHelper mHiddenHelper =
                new SelfVerifyingCallbackHelper("Hidden");
        private SelfVerifyingCallbackHelper mStateChangedHelper =
                new SelfVerifyingCallbackHelper("StateChanged");

        public TestVisibilityChangeObserver(ChromeActivity chromeActivity) {
            super(null, chromeActivity);
            mActivity = chromeActivity;
        }

        @Override
        public void onContentShown(boolean isFirstShown) {
            if (isFirstShown) {
                mInitialRevealHelper.notifyCalled();
            } else {
                mShownHelper.notifyCalled();
            }
        }

        @Override
        public void onContentHidden() {
            mHiddenHelper.notifyCalled();
        }

        @Override
        public void onContentStateChanged(int contentState) {
            mStateChangedHelper.notifyCalled();
        }

        public void expectEvents(Event... events) {
            for (Event e : events) {
                switch (e) {
                    case InitialReveal:
                        mInitialRevealHelper.waitForCallback();
                        break;
                    case Shown:
                        mShownHelper.waitForCallback();
                        break;
                    case Hidden:
                        mHiddenHelper.waitForCallback();
                        break;
                    case StateChange:
                        mStateChangedHelper.waitForCallback();
                        break;
                }
            }
            verifyCalls();
        }

        public void verifyCalls() {
            mInitialRevealHelper.verifyCallCount();
            mShownHelper.verifyCallCount();
            mHiddenHelper.verifyCallCount();
            mStateChangedHelper.verifyCallCount();
        }

        @Override
        protected boolean isObservedContentCurrent() {
            BottomSheet.BottomSheetContent currentSheet =
                    mActivity.getBottomSheet().getCurrentSheetContent();
            return currentSheet != null && currentSheet.getType() == TYPE_SUGGESTIONS;
        }
    }

    private static class SelfVerifyingCallbackHelper extends CallbackHelper {
        private final String mName;
        private int mVerifiedCallCount = 0;

        public SelfVerifyingCallbackHelper(String name) {
            mName = name;
        }

        public void waitForCallback() {
            try {
                super.waitForCallback(mName + " not called.", mVerifiedCallCount);
                mVerifiedCallCount += 1;
            } catch (InterruptedException | TimeoutException e) {
                throw new AssertionError(e);
            }
        }

        public void verifyCallCount() {
            assertEquals(mName + " call count", mVerifiedCallCount, getCallCount());
        }
    }

    private static class SpyEventReporter extends DummySuggestionsEventReporter {
        public final SelfVerifyingCallbackHelper surfaceOpenedHelper =
                new SelfVerifyingCallbackHelper("onSurfaceOpened");

        @Override
        public void onSurfaceOpened() {
            surfaceOpenedHelper.notifyCalled();
        }
    }

    // TODO(dgn): Replace with with shared one after merge.
    public static void waitForWindowUpdates() {
        final long maxWindowUpdateTimeMs = scaleTimeout(1000);
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.waitForWindowUpdate(null, maxWindowUpdateTimeMs);
        device.waitForIdle(maxWindowUpdateTimeMs);
    }
}
