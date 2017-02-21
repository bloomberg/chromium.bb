// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Intent;
import android.net.Uri;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * The base class of the WebappActivity tests. It provides the common methods to access the activity
 * UI.  This particular test base only instantiates WebappActivity0.
 */
public abstract class WebappActivityTestBase extends ChromeActivityTestCaseBase<WebappActivity0> {
    static final String WEBAPP_ID = "webapp_id";
    static final String WEBAPP_NAME = "webapp name";
    static final String WEBAPP_SHORT_NAME = "webapp short name";

    private static final long STARTUP_TIMEOUT = scaleTimeout(10000);

    // Empty 192x192 image generated with:
    // ShortcutHelper.encodeBitmapAsString(Bitmap.createBitmap(192, 192, Bitmap.Config.ARGB_4444));
    protected static final String TEST_ICON =
            "iVBORw0KGgoAAAANSUhEUgAAAMAAAADACAYAAABS3GwHAAAABHNCSVQICAgIfAhkiAAAAKZJREFU"
            + "eJztwTEBAAAAwqD1T20JT6AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD4GQN4AAe3mX6IA"
            + "AAAASUVORK5CYII=";

    // Empty 512x512 image generated with:
    // ShortcutHelper.encodeBitmapAsString(Bitmap.createBitmap(512, 512, Bitmap.Config.ARGB_4444));
    protected static final String TEST_SPLASH_ICON =
            "iVBORw0KGgoAAAANSUhEUgAAAgAAAAIACAYAAAD0eNT6AAAABHNCSVQICAgIfAhkiAAABA9JREFU"
            + "eJztwTEBAAAAwqD1T20Hb6AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
            + "AAAAAAAAAOA3AvAAAdln8YgAAAAASUVORK5CYII=";

    public WebappActivityTestBase() {
        super(WebappActivity0.class);
    }

    /**
     * Creates the Intent that starts the WebAppActivity. This is meant to be overriden by other
     * tests in order for them to pass some specific values, but it defaults to a web app that just
     * loads about:blank to avoid a network load.  This results in the URL bar showing because
     * {@link UrlUtils} cannot parse this type of URL.
     */
    protected Intent createIntent() {
        Intent intent = new Intent(getInstrumentation().getTargetContext(), WebappActivity0.class);
        intent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + WEBAPP_ID));
        intent.putExtra(ShortcutHelper.EXTRA_ID, WEBAPP_ID);
        intent.putExtra(ShortcutHelper.EXTRA_URL, "about:blank");
        intent.putExtra(ShortcutHelper.EXTRA_NAME, WEBAPP_NAME);
        intent.putExtra(ShortcutHelper.EXTRA_SHORT_NAME, WEBAPP_SHORT_NAME);
        return intent;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Register the webapp so when the data storage is opened, the test doesn't crash.
        WebappRegistry.refreshSharedPrefsForTesting();
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(WEBAPP_ID, callback);
        callback.waitForCallback(0);
        callback.getStorage().updateFromShortcutIntent(createIntent());
    }

    /**
     * Starts up the WebappActivity and sets up the test observer.
     */
    protected final void startWebappActivity() throws Exception {
        startWebappActivity(createIntent());
    }

    /**
     * Starts up the WebappActivity with a specific Intent and sets up the test observer.
     */
    protected final void startWebappActivity(Intent intent) throws Exception {
        setActivityIntent(intent);
        waitUntilIdle();
    }

    /**
     * Waits until any loads in progress have completed.
     */
    protected void waitUntilIdle() {
        getInstrumentation().waitForIdleSync();
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return getActivity().getActivityTab() != null
                            && !getActivity().getActivityTab().isLoading();
                }
            }, STARTUP_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        getInstrumentation().waitForIdleSync();
    }

    @Override
    public final void startMainActivity() throws InterruptedException {
        // Do nothing; the WebappActivity may not have been completely set up, yet.
    }

    /**
     * Waits for the splash screen to be hidden.
     */
    protected void waitUntilSplashscreenHides() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !getActivity().isSplashScreenVisibleForTests();
            }
        });
    }

    protected ViewGroup waitUntilSplashScreenAppears() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getSplashScreenForTests() != null;
            }
        });

        ViewGroup splashScreen = getActivity().getSplashScreenForTests();
        if (splashScreen == null) {
            fail("No splash screen available.");
        }
        return splashScreen;
    }
}
