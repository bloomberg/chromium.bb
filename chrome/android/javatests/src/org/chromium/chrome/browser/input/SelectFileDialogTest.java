// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.input;

import android.app.Activity;
import android.content.Intent;
import android.provider.MediaStore;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellActivity.ActivityWindowAndroidFactory;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Integration test for select file dialog used for <input type="file" />
 */
public class SelectFileDialogTest extends ChromeShellTestBase {
    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\"" +
            "content=\"width=device-width, initial-scale=2.0, maximum-scale=2.0\" /></head>" +
            "<body><form action=\"about:blank\">" +
            "<input id=\"input_file\" type=\"file\" /><br/>" +
            "<input id=\"input_image\" type=\"file\" accept=\"image/*\" capture /><br/>" +
            "<input id=\"input_audio\" type=\"file\" accept=\"audio/*\" capture />" +
            "</form>" +
            "</body></html>");

    private ContentViewCore mContentViewCore;
    private ActivityWindowAndroidForTest mActivityWindowAndroidForTest;

    private static class ActivityWindowAndroidForTest extends ActivityWindowAndroid {
        public Intent lastIntent;
        public IntentCallback lastCallback;
        /**
         * @param activity
         */
        public ActivityWindowAndroidForTest(Activity activity) {
            super(activity);
        }

        @Override
        public int showCancelableIntent(Intent intent, IntentCallback callback, int errorId) {
            lastIntent = intent;
            lastCallback = callback;
            return 1;
        }
    }

    private class IntentSentCriteria implements Criteria {
        @Override
        public boolean isSatisfied() {
            return mActivityWindowAndroidForTest.lastIntent != null;
        }
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        ChromeShellActivity.setActivityWindowAndroidFactory(new ActivityWindowAndroidFactory() {
            @Override
            public ActivityWindowAndroid getActivityWindowAndroid(Activity activity) {
                mActivityWindowAndroidForTest = new ActivityWindowAndroidForTest(activity);
                return mActivityWindowAndroidForTest;
            }
        });
        launchChromeShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());

        mContentViewCore = getActivity().getActiveContentViewCore();
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        assertWaitForPageScaleFactorMatch(2);
        assertTrue(
                DOMUtils.waitForNonZeroNodeBounds(mContentViewCore, "input_file"));
    }

    /**
     * Tests that clicks on <input type="file" /> trigger intent calls to ActivityWindowAndroid.
     */
    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testSelectFileAndCancelRequest() throws Throwable {
        DOMUtils.clickNode(this, mContentViewCore, "input_file");
        assertTrue("SelectFileDialog never sent an intent.",
                CriteriaHelper.pollForCriteria(new IntentSentCriteria()));
        assertEquals(Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();

        DOMUtils.clickNode(this, mContentViewCore, "input_image");
        assertTrue("SelectFileDialog never sent an intent.",
                CriteriaHelper.pollForCriteria(new IntentSentCriteria()));
        assertEquals(MediaStore.ACTION_IMAGE_CAPTURE,
                mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();

        DOMUtils.clickNode(this, mContentViewCore, "input_audio");
        assertTrue("SelectFileDialog never sent an intent.",
                CriteriaHelper.pollForCriteria(new IntentSentCriteria()));
        assertEquals(MediaStore.Audio.Media.RECORD_SOUND_ACTION,
                mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();
    }

    private void resetActivityWindowAndroidForTest() {
        UiUtils.runOnUiThread(getActivity(), new Runnable() {
            @Override
            public void run() {
                mActivityWindowAndroidForTest.lastCallback.onIntentCompleted(
                        mActivityWindowAndroidForTest, Activity.RESULT_CANCELED, null, null);
            }
        });
        mActivityWindowAndroidForTest.lastCallback = null;
        mActivityWindowAndroidForTest.lastIntent = null;
    }
}
