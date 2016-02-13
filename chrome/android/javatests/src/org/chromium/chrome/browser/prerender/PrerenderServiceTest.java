// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import android.os.Environment;
import android.test.suitebuilder.annotation.SmallTest;
import android.widget.EditText;

import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * A test suite for the ChromeBrowserPrerenderService. This makes sure the service initializes the
 * browser process and the UI and also can carry out prerendering related operations without causing
 * any issues with Chrome launching from external intents afterwards.
 */
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class PrerenderServiceTest extends
        BaseActivityInstrumentationTestCase<PrerenderAPITestActivity> {

    public PrerenderServiceTest() {
        super(PrerenderAPITestActivity.class);
    }

    private static final String GOOGLE_PATH = "/chrome/test/data/android/prerender/google.html";
    private static final String YOUTUBE_PATH = "/chrome/test/data/android/prerender/youtube.html";
    private static final String REDIRECT_GOOGLE_PATH =
            "/chrome/test/data/android/prerender/google_redirect.html";

    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        setActivityInitialTouchMode(true);
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Test that binding and initializing the UI works and Chrome can be launched
     * without issues afterwards.
     * @throws InterruptedException
     */
    @SmallTest
    @DisabledTest
    @Feature({"PrerenderService"})
    public void testBindingAndInitializing() throws InterruptedException {
        if (SysUtils.isLowEndDevice()) return;
        ensureBindingAndInitializingUI();
        loadChromeWithUrl(mTestServer.getURL(GOOGLE_PATH));
    }

    /**
     * Test that a prerender request can be sent via the service and Chrome can be launched
     * from that same url without any issues. This doesn't test whether the url has actually
     * finished prerendering.
     * @throws InterruptedException
     */
    /*
     * @Feature({"PrerenderService"})
     * @SmallTest
     */
    @DisabledTest
    public void testPrerenderingSameUrl() throws InterruptedException {
        if (SysUtils.isLowEndDevice()) return;
        ensureBindingAndInitializingUI();
        ensurePrerendering(mTestServer.getURL(GOOGLE_PATH));
        loadChromeWithUrl(mTestServer.getURL(GOOGLE_PATH));
    }

    /**
     * Test that a prerender request can be sent via the service and Chrome can be launched
     * from that different url without any issues. This doesn't test whether the url has actually
     * finished prerendering.
     * @throws InterruptedException
     */
    @SmallTest
    @DisabledTest
    @Feature({"PrerenderService"})
    public void testPrerenderingDifferentUrl() throws InterruptedException {
        if (SysUtils.isLowEndDevice()) return;
        ensureBindingAndInitializingUI();
        ensurePrerendering(mTestServer.getURL(GOOGLE_PATH));
        loadChromeWithUrl(mTestServer.getURL(YOUTUBE_PATH));
    }

    /**
     * Test that a prerender request with a redirect can be sent via the service and the final
     * target url can still be used to fetch that prerendered page.
     * @throws Exception
     */
    @SmallTest
    @DisabledTest
    @Feature({"PrerenderService"})
    public void testPrerenderingRedirectUrl() throws Exception {
        if (SysUtils.isLowEndDevice()) return;
        ensureBindingAndInitializingUI();
        ensurePrerendering(mTestServer.getURL(REDIRECT_GOOGLE_PATH));
        assertServiceHasPrerenderedUrl(mTestServer.getURL(GOOGLE_PATH));
    }

    private void ensureBindingAndInitializingUI() {
        assertNotNull(getActivity());
        // TODO(yusufo): Add a check for native library loaded notification being received.
    }

    private void ensurePrerendering(final String url) throws InterruptedException {
        ensureBindingAndInitializingUI();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ((EditText) getActivity().findViewById(R.id.url_to_load)).setText(url);
            }
        });
        TouchCommon.singleClickView(getActivity().findViewById(R.id.preload_button));
        assertServiceHasPrerenderedUrl(url);
    }

    private void loadChromeWithUrl(final String url) throws InterruptedException {
        assertNotNull(getActivity());
        ThreadUtils.runOnUiThreadBlocking(new Runnable(){
            @Override
            public void run() {
                ((EditText) getActivity().findViewById(R.id.url_to_load)).setText(url);
            }
        });
        final ChromeActivity chromeActivity = ActivityUtils.waitForActivity(
                getInstrumentation(),
                FeatureUtilities.isDocumentMode(getActivity())
                        ? DocumentActivity.class : ChromeTabbedActivity.class, new Runnable() {
                            @Override
                            public void run() {
                                TouchCommon.singleClickView(getActivity().findViewById(
                                        R.id.load_button));
                            }
                        });
        // TODO(yusufo): We should be using the NotificationCenter for checking the page loading.
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return chromeActivity.getActivityTab() != null
                        && chromeActivity.getActivityTab().isLoadingAndRenderingDone();
            }
        });
        assertTrue(chromeActivity.getActivityTab().getUrl().equals(url));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertFalse(WarmupManager.getInstance().hasAnyPrerenderedUrl());
            }
        });
    }

    private void assertServiceHasPrerenderedUrl(final String url) throws InterruptedException {
        CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return WarmupManager.getInstance().hasPrerenderedUrl(url);
            }
        });
    }
}
