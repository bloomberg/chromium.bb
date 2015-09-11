// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.app.Dialog;
import android.support.v7.app.AlertDialog;
import android.test.suitebuilder.annotation.MediumTest;
import android.widget.Button;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.Arrays;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.Set;

/**
 * Peforms integration tests with ClearBrowsingDataDialogFragment.
 */
public class ClearBrowsingDataDialogFragmentTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {

    private TestClearDataDialogFragment mFragment;
    private boolean mCallbackCalled;

    public ClearBrowsingDataDialogFragmentTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    public void testClearingSiteDataClearsWebapps() throws Exception {
        mFragment = new TestClearDataDialogFragment(EnumSet.of(
                ClearBrowsingDataDialogFragment.DialogOption.CLEAR_COOKIES_AND_SITE_DATA));

        WebappRegistry.registerWebapp(getActivity(), "first");
        WebappRegistry.getRegisteredWebappIds(getActivity(), new WebappRegistry.FetchCallback() {
            @Override
            public void onWebappIdsRetrieved(Set<String> ids) {
                assertEquals(new HashSet<String>(Arrays.asList("first")), ids);
                mCallbackCalled = true;
            }
        });
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        }));
        mCallbackCalled = false;

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mFragment.show(getActivity().getFragmentManager(),
                        ClearBrowsingDataDialogFragment.FRAGMENT_TAG);
            }
        });
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mFragment.getDialog() != null;
            }
        }));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Dialog dialog = mFragment.getDialog();
                Button clearButton = ((AlertDialog) dialog).getButton(AlertDialog.BUTTON_POSITIVE);
                clearButton.performClick();
            }
        });
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mFragment.getProgressDialog() == null;
            }
        }));

        WebappRegistry.getRegisteredWebappIds(getActivity(), new WebappRegistry.FetchCallback() {
            @Override
            public void onWebappIdsRetrieved(Set<String> ids) {
                assertTrue(ids.isEmpty());
                mCallbackCalled = true;
            }
        });
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mCallbackCalled;
            }
        }));
    }

    private static class TestClearDataDialogFragment extends ClearBrowsingDataDialogFragment {
        private final EnumSet<DialogOption> mDefaultOptions;

        public TestClearDataDialogFragment(EnumSet<DialogOption> defaultOptions) {
            mDefaultOptions = defaultOptions;
        }

        @Override
        protected EnumSet<DialogOption> getDefaultDialogOptionsSelections() {
            return mDefaultOptions;
        }
    }
}
