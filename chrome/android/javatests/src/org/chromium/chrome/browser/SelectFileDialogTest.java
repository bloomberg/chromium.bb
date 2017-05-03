// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.provider.MediaStore;
import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.SelectFileDialog;

/**
 * Integration test for select file dialog used for <input type="file" />
 */
public class SelectFileDialogTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=2.0, maximum-scale=2.0\" /></head>"
            + "<body><form action=\"about:blank\">"
            + "<input id=\"input_file\" type=\"file\" /><br/>"
            + "<input id=\"input_text\" type=\"file\" accept=\"text/plain\" /><br/>"
            + "<input id=\"input_any\" type=\"file\" accept=\"*/*\" /><br/>"
            + "<input id=\"input_file_multiple\" type=\"file\" multiple /><br />"
            + "<input id=\"input_image\" type=\"file\" accept=\"image/*\" capture /><br/>"
            + "<input id=\"input_audio\" type=\"file\" accept=\"audio/*\" capture />"
            + "</form>"
            + "</body></html>");

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
        public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
            lastIntent = intent;
            lastCallback = callback;
            return 1;
        }

        @Override
        public boolean canResolveActivity(Intent intent) {
            return true;
        }
    }

    private class IntentSentCriteria extends Criteria {
        public IntentSentCriteria() {
            super("SelectFileDialog never sent an intent.");
        }

        @Override
        public boolean isSatisfied() {
            return mActivityWindowAndroidForTest.lastIntent != null;
        }
    }

    private ContentViewCore mContentViewCore;
    private ActivityWindowAndroidForTest mActivityWindowAndroidForTest;

    public SelectFileDialogTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(DATA_URL);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mActivityWindowAndroidForTest = new ActivityWindowAndroidForTest(getActivity());
        SelectFileDialog.setWindowAndroidForTests(mActivityWindowAndroidForTest);

        mContentViewCore = getActivity().getCurrentContentViewCore();
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        assertWaitForPageScaleFactorMatch(2);
        DOMUtils.waitForNonZeroNodeBounds(mContentViewCore.getWebContents(), "input_file");
    }

    /**
     * Tests that clicks on <input type="file" /> trigger intent calls to ActivityWindowAndroid.
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @MediumTest
    @Feature({"TextInput", "Main"})
    @RetryOnFailure
    public void testSelectFileAndCancelRequest() throws Throwable {
        {
            DOMUtils.clickNode(mContentViewCore, "input_file");
            CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
            assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent) mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                            Intent.EXTRA_INTENT);
            assertNotNull(contentIntent);
            assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mContentViewCore, "input_text");
            CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
            assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent) mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                            Intent.EXTRA_INTENT);
            assertNotNull(contentIntent);
            assertTrue(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mContentViewCore, "input_any");
            CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
            assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent) mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                            Intent.EXTRA_INTENT);
            assertNotNull(contentIntent);
            assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            resetActivityWindowAndroidForTest();
        }

        {
            DOMUtils.clickNode(mContentViewCore, "input_file_multiple");
            CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
            assertEquals(
                    Intent.ACTION_CHOOSER, mActivityWindowAndroidForTest.lastIntent.getAction());
            Intent contentIntent =
                    (Intent) mActivityWindowAndroidForTest.lastIntent.getParcelableExtra(
                            Intent.EXTRA_INTENT);
            assertNotNull(contentIntent);
            assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                assertTrue(contentIntent.hasExtra(Intent.EXTRA_ALLOW_MULTIPLE));
            }
            resetActivityWindowAndroidForTest();
        }

        DOMUtils.clickNode(mContentViewCore, "input_image");
        CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
        assertEquals(MediaStore.ACTION_IMAGE_CAPTURE,
                mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();

        DOMUtils.clickNode(mContentViewCore, "input_audio");
        CriteriaHelper.pollInstrumentationThread(new IntentSentCriteria());
        assertEquals(MediaStore.Audio.Media.RECORD_SOUND_ACTION,
                mActivityWindowAndroidForTest.lastIntent.getAction());
        resetActivityWindowAndroidForTest();
    }

    private void resetActivityWindowAndroidForTest() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivityWindowAndroidForTest.lastCallback.onIntentCompleted(
                        mActivityWindowAndroidForTest, Activity.RESULT_CANCELED, null);
            }
        });
        mActivityWindowAndroidForTest.lastCallback = null;
        mActivityWindowAndroidForTest.lastIntent = null;
    }
}
