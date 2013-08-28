// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.os.Parcel;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Log;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.testshell.ChromiumTestShellActivity;
import org.chromium.chrome.testshell.ChromiumTestShellApplication;
import org.chromium.chrome.testshell.ChromiumTestShellApplicationObserver;
import org.chromium.chrome.testshell.ChromiumTestShellTestBase;
import org.chromium.chrome.testshell.TestShellTab;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

public class ShortcutHelperTest extends ChromiumTestShellTestBase {
    private static final String WEBAPP_PACKAGE_NAME = "packageName";
    private static final String WEBAPP_CLASS_NAME = "className";

    private static final String WEBAPP_TITLE = "Webapp shortcut";
    private static final String WEBAPP_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable</body></html>");

    private static final String SECOND_WEBAPP_TITLE = "Webapp shortcut #2";
    private static final String SECOND_WEBAPP_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + SECOND_WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable again</body></html>");

    private static final String NORMAL_TITLE = "Plain shortcut";
    private static final String NORMAL_HTML = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "<head><title>" + NORMAL_TITLE + "</title></head>"
            + "<body>Not Webapp capable</body></html>");

    private static class TestObserver implements ChromiumTestShellApplicationObserver {
        Intent firedIntent;

        @Override
        public boolean onSendBroadcast(Intent intent) {
            if (intent.hasExtra(Intent.EXTRA_SHORTCUT_NAME)) {
                // Stop a shortcut from really being added.
                firedIntent = intent;
                return false;
            }

            return true;
        }

        public void reset() {
            firedIntent = null;
        }
    }

    private ChromiumTestShellActivity mActivity;
    private TestObserver mTestObserver;

    @Override
    public void setUp() throws Exception {
        ShortcutHelper.setWebappActivityInfo(WEBAPP_PACKAGE_NAME, WEBAPP_CLASS_NAME);
        mActivity = launchChromiumTestShellWithBlankPage();

        // Set up the observer.
        mTestObserver = new TestObserver();
        ChromiumTestShellApplication application =
                (ChromiumTestShellApplication) mActivity.getApplication();
        application.addObserver(mTestObserver);

        super.setUp();
    }

    @MediumTest
    @Feature("{Webapp}")
    public void testAddWebappShortcuts() throws InterruptedException {
        // Add a webapp shortcut and make sure the intent's parameters make sense.
        addShortcutToURL(WEBAPP_HTML);
        Intent firedIntent = mTestObserver.firedIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(WEBAPP_HTML, launchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_PACKAGE_NAME, launchIntent.getComponent().getPackageName());
        assertEquals(WEBAPP_CLASS_NAME, launchIntent.getComponent().getClassName());

        // Add a second shortcut and make sure it matches the second webapp's parameters.
        mTestObserver.reset();
        addShortcutToURL(SECOND_WEBAPP_HTML);
        Intent newFiredIntent = mTestObserver.firedIntent;
        assertEquals(SECOND_WEBAPP_TITLE,
                newFiredIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent newLaunchIntent = newFiredIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(SECOND_WEBAPP_HTML, newLaunchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_PACKAGE_NAME, newLaunchIntent.getComponent().getPackageName());
        assertEquals(WEBAPP_CLASS_NAME, newLaunchIntent.getComponent().getClassName());
    }

    @MediumTest
    @Feature("{Webapp}")
    public void testAddBookmarkShortcut() throws InterruptedException {
        addShortcutToURL(NORMAL_HTML);

        // Make sure the intent's parameters make sense.
        Intent firedIntent = mTestObserver.firedIntent;
        assertEquals(NORMAL_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(Intent.ACTION_VIEW, launchIntent.getAction());
        assertEquals(NORMAL_HTML, launchIntent.getDataString());
        assertNull(launchIntent.getComponent());
    }

    private void addShortcutToURL(String url) throws InterruptedException {
        loadUrlWithSanitization(url);
        assertTrue(waitForActiveShellToBeDoneLoading());

        // Add the shortcut.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ShortcutHelper.addShortcut(mActivity.getActiveTab());
            }
        });

        // Make sure that the shortcut was added.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mTestObserver.firedIntent != null;
            }
        }));
    }
}