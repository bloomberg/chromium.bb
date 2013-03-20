// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test;

import android.app.Activity;
import android.content.ContentProvider;
import android.content.ContentResolver;
import android.os.SystemClock;
import android.test.ActivityInstrumentationTestCase2;
import android.test.IsolatedContext;
import android.test.mock.MockContentResolver;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Pair;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeBrowserProvider;
import org.chromium.chrome.browser.autofill.AutofillListAdapter;
import org.chromium.chrome.browser.autofill.AutofillPopup;
import org.chromium.chrome.browser.autofill.AutofillPopup.AutofillPopupDelegate;
import org.chromium.chrome.browser.autofill.AutofillSuggestion;
import org.chromium.chrome.testshell.ChromiumTestShellActivity;
import org.chromium.chrome.testshell.ChromiumTestShellTestBase;
import org.chromium.content.browser.ContainerViewDelegate;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.ui.gfx.ActivityNativeWindow;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests the Autofill's java code for creating the AutofillPopup object, opening and selecting
 * popups.
 */
public class AutofillTest extends ChromiumTestShellTestBase {

    private AutofillPopup mAutofillPopup;
    private ActivityNativeWindow mNativeWindow;
    private MockAutofillCallback mMockAutofillCallback;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        ChromiumTestShellActivity activity = launchChromiumTestShellWithBlankPage();
        assertNotNull(activity);
        waitForActiveShellToBeDoneLoading();

        mMockAutofillCallback = new MockAutofillCallback();
        mNativeWindow = new ActivityNativeWindow(activity);
        final ContainerViewDelegate viewDelegate =
                activity.getActiveContentView().getContentViewCore().getContainerViewDelegate();

        UiUtils.runOnUiThread(getActivity(), new Runnable() {
            @Override
            public void run() {
                mAutofillPopup = new AutofillPopup(mNativeWindow,
                        viewDelegate,
                        mMockAutofillCallback);
                mAutofillPopup.setAnchorRect(50, 500, 500, 50);
            }
        });

    }

    private class MockAutofillCallback implements AutofillPopupDelegate{
        private static final int CALLBACK_TIMEOUT_MS = 4000;
        private static final int CHECK_INTERVAL_MS = 100;
        private final AtomicBoolean mGotPopupSelection = new AtomicBoolean(false);
        public int mListIndex = -1;

        @Override
        public void suggestionSelected(int listIndex) {
            mListIndex = listIndex;
            mAutofillPopup.dismiss();
            mGotPopupSelection.set(true);
        }

        public boolean waitForCallback() throws InterruptedException {
            return CriteriaHelper.pollForCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mGotPopupSelection.get();
                }
            }, CALLBACK_TIMEOUT_MS, CHECK_INTERVAL_MS);
        }

        @Override
        public void requestHide() {
        }
    }

    private AutofillSuggestion[] createTwoAutofillSuggestionArray() {
        return new AutofillSuggestion[] {
            new AutofillSuggestion("Sherlock Holmes", "221B Baker Street", 42),
            new AutofillSuggestion("Arthur Dent", "West Country", 43),
        };
    }

    private AutofillSuggestion[] createFiveAutofillSuggestionArray() {
        return new AutofillSuggestion[] {
            new AutofillSuggestion("Sherlock Holmes", "221B Baker Street", 42),
            new AutofillSuggestion("Arthur Dent", "West Country", 43),
            new AutofillSuggestion("Arthos", "France", 44),
            new AutofillSuggestion("Porthos", "France", 45),
            new AutofillSuggestion("Aramis", "France", 46),
        };
    }

    public boolean openAutofillPopupAndWaitUntilReady(final AutofillSuggestion[] suggestions)
            throws InterruptedException {
        UiUtils.runOnUiThread(getActivity(), new Runnable() {
            @Override
            public void run() {
                mAutofillPopup.show(suggestions);
            }
        });
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mAutofillPopup.getListView().getChildCount() > 0;
            }
        });
    }

    @SmallTest
    @Feature({"autofill"})
    public void testAutofillWithDifferentNumberSuggestions() throws Exception {
        assertTrue(openAutofillPopupAndWaitUntilReady(createTwoAutofillSuggestionArray()));
        assertEquals(2, mAutofillPopup.getListView().getCount());

        assertTrue(openAutofillPopupAndWaitUntilReady(createFiveAutofillSuggestionArray()));
        assertEquals(5, mAutofillPopup.getListView().getCount());
    }

    @SmallTest
    @Feature({"autofill"})
    public void testAutofillClickFirstSuggestion() throws Exception {
        AutofillSuggestion[] suggestions = createTwoAutofillSuggestionArray();
        assertTrue(openAutofillPopupAndWaitUntilReady(suggestions));
        assertEquals(2, mAutofillPopup.getListView().getCount());

        TouchCommon touchCommon = new TouchCommon(this);
        touchCommon.singleClickViewRelative(mAutofillPopup.getListView(), 10, 10);
        assertTrue(mMockAutofillCallback.waitForCallback());

        assertEquals(0, mMockAutofillCallback.mListIndex);
    }
}
