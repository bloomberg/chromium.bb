// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.test.FlakyTest;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellApplication;
import org.chromium.chrome.shell.ChromeShellApplicationObserver;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

public class ShortcutHelperTest extends ChromeShellTestBase {
    private static final String WEBAPP_ACTION_NAME = "WEBAPP_ACTION";

    private static final String WEBAPP_TITLE = "Webapp shortcut";
    private static final String WEBAPP_HTML = UrlUtils.encodeHtmlDataUri(
            "<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable</body></html>");
    private static final String EDITED_WEBAPP_TITLE = "Webapp shortcut edited";

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

    private static class TestObserver implements ChromeShellApplicationObserver {
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

    private ChromeShellActivity mActivity;
    private TestObserver mTestObserver;

    @Override
    public void setUp() throws Exception {
        ShortcutHelper.setFullScreenAction(WEBAPP_ACTION_NAME);
        mActivity = launchChromeShellWithBlankPage();

        // Set up the observer.
        mTestObserver = new TestObserver();
        ChromeShellApplication application =
                (ChromeShellApplication) mActivity.getApplication();
        application.addObserver(mTestObserver);

        super.setUp();
    }

    /**
     * @MediumTest
     * @Feature("{Webapp}")
     * crbug.com/303486
     */
    @FlakyTest
    public void testAddWebappShortcuts() throws InterruptedException {
        // Add a webapp shortcut and make sure the intent's parameters make sense.
        addShortcutToURL(WEBAPP_HTML, "");
        Intent firedIntent = mTestObserver.firedIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(WEBAPP_HTML, launchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_ACTION_NAME, launchIntent.getAction());
        assertEquals(mActivity.getPackageName(), launchIntent.getPackage());

        // Add a second shortcut and make sure it matches the second webapp's parameters.
        mTestObserver.reset();
        addShortcutToURL(SECOND_WEBAPP_HTML, "");
        Intent newFiredIntent = mTestObserver.firedIntent;
        assertEquals(SECOND_WEBAPP_TITLE,
                newFiredIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent newLaunchIntent = newFiredIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(SECOND_WEBAPP_HTML, newLaunchIntent.getStringExtra(ShortcutHelper.EXTRA_URL));
        assertEquals(WEBAPP_ACTION_NAME, newLaunchIntent.getAction());
        assertEquals(mActivity.getPackageName(), newLaunchIntent.getPackage());
    }

    /**
     * @MediumTest
     * @Feature("{Webapp}")
     * crbug.com/303486
     */
    @FlakyTest
    public void testAddBookmarkShortcut() throws InterruptedException {
        addShortcutToURL(NORMAL_HTML, "");

        // Make sure the intent's parameters make sense.
        Intent firedIntent = mTestObserver.firedIntent;
        assertEquals(NORMAL_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));

        Intent launchIntent = firedIntent.getParcelableExtra(Intent.EXTRA_SHORTCUT_INTENT);
        assertEquals(mActivity.getPackageName(), launchIntent.getPackage());
        assertEquals(Intent.ACTION_VIEW, launchIntent.getAction());
        assertEquals(NORMAL_HTML, launchIntent.getDataString());
    }

    /**
     * @MediumTest
     * @Feature("{Webapp}")
     * crbug.com/303486
     */
    @FlakyTest
    public void testAddWebappShortcutsWithoutTitleEdit() throws InterruptedException {
        // Add a webapp shortcut to check unedited title.
        addShortcutToURL(WEBAPP_HTML, "");
        Intent firedIntent = mTestObserver.firedIntent;
        assertEquals(WEBAPP_TITLE, firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    /**
     * @MediumTest
     * @Feature("{Webapp}")
     * crbug.com/303486
     */
    @FlakyTest
    public void testAddWebappShortcutsWithTitleEdit() throws InterruptedException {
        // Add a webapp shortcut to check edited title.
        addShortcutToURL(WEBAPP_HTML, EDITED_WEBAPP_TITLE);
        Intent firedIntent = mTestObserver.firedIntent;
        assertEquals(EDITED_WEBAPP_TITLE , firedIntent.getStringExtra(Intent.EXTRA_SHORTCUT_NAME));
    }

    private void addShortcutToURL(String url, final String title) throws InterruptedException {
        loadUrlWithSanitization(url);
        assertTrue(waitForActiveShellToBeDoneLoading());

        // Add the shortcut.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ShortcutHelper.addShortcut(mActivity.getApplicationContext(),
                        mActivity.getActiveTab(), title);
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