// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.test.suitebuilder.annotation.SmallTest;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.autofill.AutofillDelegate;
import org.chromium.ui.autofill.AutofillPopup;
import org.chromium.ui.autofill.AutofillSuggestion;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests the Autofill's java code for creating the AutofillPopup object, opening and selecting
 * popups.
 */
public class AutofillTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private AutofillPopup mAutofillPopup;
    private WindowAndroid mWindowAndroid;
    private MockAutofillCallback mMockAutofillCallback;

    public AutofillTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SuppressFBWarnings("URF_UNREAD_FIELD")
    @Override
    public void setUp() throws Exception {
        super.setUp();

        final ChromeActivity activity = getActivity();
        mMockAutofillCallback = new MockAutofillCallback();
        final ViewAndroidDelegate viewDelegate =
                activity.getCurrentContentViewCore().getViewAndroidDelegate();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mWindowAndroid = new ActivityWindowAndroid(activity);
                mAutofillPopup = new AutofillPopup(activity, viewDelegate, mMockAutofillCallback);
                mAutofillPopup.filterAndShow(new AutofillSuggestion[0], false);
                mAutofillPopup.setAnchorRect(50, 500, 500, 50);
            }
        });
    }

    private static final long CALLBACK_TIMEOUT_MS = scaleTimeout(4000);
    private static final int CHECK_INTERVAL_MS = 100;

    private class MockAutofillCallback implements AutofillDelegate {
        private final AtomicBoolean mGotPopupSelection = new AtomicBoolean(false);
        public int mListIndex = -1;

        @Override
        public void suggestionSelected(int listIndex) {
            mListIndex = listIndex;
            mAutofillPopup.dismiss();
            mGotPopupSelection.set(true);
        }

        @Override
        public void deleteSuggestion(int listIndex) {
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
        public void dismissed() {
        }
    }

    private AutofillSuggestion[] createTwoAutofillSuggestionArray() {
        return new AutofillSuggestion[] {
            new AutofillSuggestion("Sherlock Holmes", "221B Baker Street", DropdownItem.NO_ICON,
                42, false),
            new AutofillSuggestion("Arthur Dent", "West Country", DropdownItem.NO_ICON, 43, false),
        };
    }

    private AutofillSuggestion[] createFiveAutofillSuggestionArray() {
        return new AutofillSuggestion[] {
            new AutofillSuggestion("Sherlock Holmes", "221B Baker Street", DropdownItem.NO_ICON,
                42, false),
            new AutofillSuggestion("Arthur Dent", "West Country", DropdownItem.NO_ICON, 43, false),
            new AutofillSuggestion("Arthos", "France", DropdownItem.NO_ICON, 44, false),
            new AutofillSuggestion("Porthos", "France", DropdownItem.NO_ICON, 45, false),
            new AutofillSuggestion("Aramis", "France", DropdownItem.NO_ICON, 46, false),
        };
    }

    public boolean openAutofillPopupAndWaitUntilReady(final AutofillSuggestion[] suggestions)
            throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAutofillPopup.filterAndShow(suggestions, false);
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

        TouchCommon.singleClickView(mAutofillPopup.getListView().getChildAt(0));
        assertTrue(mMockAutofillCallback.waitForCallback());

        assertEquals(0, mMockAutofillCallback.mListIndex);
    }
}
