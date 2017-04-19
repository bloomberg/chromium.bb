// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.LargeTest;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink.mojom.document_metadata.Entity;
import org.chromium.blink.mojom.document_metadata.Property;
import org.chromium.blink.mojom.document_metadata.Values;
import org.chromium.blink.mojom.document_metadata.WebPage;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.mojom.Url;

import java.util.concurrent.TimeoutException;

/**
 * Tests Copyless Paste AppIndexing using instrumented tests.
 */
@CommandLineFlags.Add("enable-features=CopylessPaste")
@Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
public class CopylessPasteTest extends ChromeTabbedActivityTestBase {
    // NODATA_PAGE doesn't contain desired metadata.
    private static final String NODATA_PAGE = "/chrome/test/data/android/about.html";

    // DATA_PAGE contains desired metadata.
    private static final String DATA_PAGE = "/chrome/test/data/android/appindexing/json-ld.html";

    private EmbeddedTestServer mTestServer;
    private CopylessHelper mCallbackHelper;

    @Override
    protected void setUp() throws Exception {
        // We have to set up the test server before starting the activity.
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());

        mCallbackHelper = new CopylessHelper();

        AppIndexingUtil.setCallbackForTesting(new Callback<WebPage>() {
            @Override
            public void onResult(WebPage webpage) {
                mCallbackHelper.notifyCalled(webpage);
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(true);
            }
        });
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(false);
            }
        });
        AppIndexingUtil.setCallbackForTesting(null);
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private static class CopylessHelper extends CallbackHelper {
        private WebPage mWebPage;

        public WebPage getWebPage() {
            return mWebPage;
        }

        public void notifyCalled(WebPage page) {
            mWebPage = page;
            notifyCalled();
        }
    }

    /**
     * Tests that CopylessPaste is disabled in Incognito tabs.
     */
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testIncognito() throws InterruptedException, TimeoutException {
        // Incognito tabs are ignored.
        newIncognitoTabsFromMenu(1);
        loadUrl(mTestServer.getURL(NODATA_PAGE));
        loadUrl(mTestServer.getURL(DATA_PAGE));
        ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());
        assertEquals(0, mCallbackHelper.getCallCount());
    }

    /**
     * Tests that CopylessPaste skips invalid schemes.
     */
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testInvalidScheme() throws InterruptedException, TimeoutException {
        // CopylessPaste only parses http and https.
        loadUrl("chrome://newtab");
        loadUrl("chrome://about");
        assertEquals(0, mCallbackHelper.getCallCount());
    }

    /**
     * Tests that CopylessPaste works on pages without desired metadata.
     */
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testNoMeta() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(NODATA_PAGE));
        mCallbackHelper.waitForCallback(0);
        assertNull(mCallbackHelper.getWebPage());
    }

    /**
     * Tests that CopylessPaste works end-to-end.
     */
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testValid() throws InterruptedException, TimeoutException {
        loadUrl(mTestServer.getURL(DATA_PAGE));
        mCallbackHelper.waitForCallback(0);
        WebPage extracted = mCallbackHelper.getWebPage();

        WebPage expected = new WebPage();
        expected.url = new Url();
        expected.url.url = mTestServer.getURL(DATA_PAGE);
        expected.title = "JSON-LD for AppIndexing Test";
        Entity e = new Entity();
        e.type = "Hotel";
        e.properties = new Property[2];
        e.properties[0] = new Property();
        e.properties[0].name = "@context";
        e.properties[0].values = new Values();
        e.properties[0].values.setStringValues(new String[] {"http://schema.org"});
        e.properties[1] = new Property();
        e.properties[1].name = "name";
        e.properties[1].values = new Values();
        e.properties[1].values.setStringValues(new String[] {"Hotel Name"});
        expected.entities = new Entity[1];
        expected.entities[0] = e;
        assertEquals(expected, extracted);
    }

    /**
     * Tests that CopylessPaste skips parsing visited pages.
     */
    @LargeTest
    @Feature({"CopylessPaste"})
    @DisabledTest(message = "Flaky: crbug.com/713172")
    public void testCache() throws InterruptedException, TimeoutException {
        // The URLs used here should be unique in CopylessPasteTest.
        String uniqueTag = "#123";
        // NODATA_PAGE doesn't contain desired metadata.
        loadUrl(mTestServer.getURL(NODATA_PAGE + uniqueTag));
        mCallbackHelper.waitForCallback(0);
        // DATA_PAGE contains desired metadata.
        loadUrl(mTestServer.getURL(DATA_PAGE + uniqueTag));
        mCallbackHelper.waitForCallback(1);

        // Cache hit without entities. Shouldn't parse again.
        loadUrl(mTestServer.getURL(NODATA_PAGE + uniqueTag));
        // Cache hit with entities. Shouldn't parse again.
        loadUrl(mTestServer.getURL(DATA_PAGE + uniqueTag));
        assertEquals(2, mCallbackHelper.getCallCount());
    }
}
