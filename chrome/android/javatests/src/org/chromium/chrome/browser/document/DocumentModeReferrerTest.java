// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Instrumentation;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Tests that Document mode handles referrers correctly.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@DisableInTabbedMode
public class DocumentModeReferrerTest extends DocumentModeTestBase {
    private static final int ACTIVITY_START_TIMEOUT = 1000;

    private String mUrl;
    private String mReferrer;
    private TabModelSelectorTabObserver mObserver;

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        if (mObserver != null) {
            mObserver.destroy();
            mObserver = null;
        }
    }

    @MediumTest
    public void testReferrerExtra() throws Exception {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        ApplicationTestUtils.launchChrome(mContext);

        // Wait for tab model to become initialized.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ChromeApplication.isDocumentTabModelSelectorInitializedForTests();
            }
        }));

        DocumentTabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
        mObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                mUrl = params.getUrl();
                if (params.getReferrer() != null) {
                    mReferrer = params.getReferrer().getUrl();
                }
            }
        };

        // Wait for document activity from the main intent to launch before firing the next
        // intent.
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT);
        assertNotNull("DocumentActivity did not start", doc);

        mUrl = null;
        mReferrer = null;

        // Fire the Intent with EXTRA_REFERRER.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(URL_2));
        IntentHandler.addTrustedIntentExtras(intent, mContext);
        mContext.startActivity(intent);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return URL_1.equals(mUrl);
            }
        }));

        assertEquals(URL_2, mReferrer);
    }

    @MediumTest
    public void testReferrerExtraAndroidApp() throws Exception {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        ApplicationTestUtils.launchChrome(mContext);

        // Wait for tab model to become initialized.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ChromeApplication.isDocumentTabModelSelectorInitializedForTests();
            }
        }));

        DocumentTabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
        mObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                mUrl = params.getUrl();
                if (params.getReferrer() != null) {
                    mReferrer = params.getReferrer().getUrl();
                }
            }
        };

        // Wait for document activity from the main intent to launch before firing the next
        // intent.
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT);
        assertNotNull("DocumentActivity did not start", doc);

        mUrl = null;
        mReferrer = null;

        // Fire the Intent with EXTRA_REFERRER using android-app scheme.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final String androidAppReferrer = "android-app://com.imdb.mobile/http/www.imdb.com";
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(androidAppReferrer));
        mContext.startActivity(intent);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return URL_1.equals(mUrl);
            }
        }));

        assertEquals(androidAppReferrer, mReferrer);
    }

    @MediumTest
    public void testReferrerExtraNotAndroidApp() throws Exception {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        ApplicationTestUtils.launchChrome(mContext);

        // Wait for tab model to become initialized.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ChromeApplication.isDocumentTabModelSelectorInitializedForTests();
            }
        }));

        DocumentTabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
        mObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                mUrl = params.getUrl();
                if (params.getReferrer() != null) {
                    mReferrer = params.getReferrer().getUrl();
                }
            }
        };

        // Wait for document activity from the main intent to launch before firing the next
        // intent.
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT);
        assertNotNull("DocumentActivity did not start", doc);

        mUrl = null;
        mReferrer = null;

        // Fire the third-party Intent with EXTRA_REFERRER using a regular URL.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final String nonAppExtra = "http://www.imdb.com";
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(nonAppExtra));
        mContext.startActivity(intent);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return URL_1.equals(mUrl);
            }
        }));

        // Check that referrer is not carried over
        assertNull(mReferrer);
    }

    @MediumTest
    public void testReferrerExtraFromExternalIntent() throws Exception {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        ApplicationTestUtils.launchChrome(mContext);

        // Wait for tab model to become initialized.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ChromeApplication.isDocumentTabModelSelectorInitializedForTests();
            }
        }));

        DocumentTabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
        mObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                mUrl = params.getUrl();
                if (params.getReferrer() != null) {
                    mReferrer = params.getReferrer().getUrl();
                }
            }
        };

        // Wait for document activity from the main intent to launch before firing the next
        // intent.
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT);
        assertNotNull("DocumentActivity did not start", doc);

        mUrl = null;
        mReferrer = null;

        // Simulate a link click inside the application that goes through external navigation
        // handler and then goes back to Chrome.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        IntentHandler.setPendingReferrer(intent, "http://www.google.com");
        mContext.startActivity(intent);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return URL_1.equals(mUrl);
            }
        }));

        // Check that referrer is not carried over
        assertEquals("http://www.google.com", mReferrer);
    }
}
