// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.TestContentProvider;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.Callable;


/** Test suite for different Android URL schemes. */
public class UrlSchemeTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public UrlSchemeTest() {
        super(ChromeActivity.class);
    }

    /**
     * Test that resource request count in the content provider works.
     * This is to make sure that attempts to access the content provider
     * will be detected.
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testContentProviderResourceRequestCount() throws IOException {
        String resource = "test_reset";
        resetResourceRequestCountInContentProvider(resource);
        ensureResourceRequestCountInContentProvider(resource, 0);
        // Make a request to the content provider.
        Uri uri = Uri.parse(createContentUrl(resource));
        Context context = getInstrumentation().getContext();
        InputStream inputStream = null;
        try {
            inputStream = context.getContentResolver().openInputStream(uri);
            assertNotNull(inputStream);
        } finally {
            if (inputStream != null) inputStream.close();
        }
        ensureResourceRequestCountInContentProvider(resource, 1);
        resetResourceRequestCountInContentProvider(resource);
        ensureResourceRequestCountInContentProvider(resource, 0);
    }

    /**
     * Make sure content URL access works.
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlAccess() throws InterruptedException {
        String resource = "content_disabled_by_default";
        resetResourceRequestCountInContentProvider(resource);
        loadUrl(createContentUrl(resource));
        ensureResourceRequestCountInContentProviderNotLessThan(resource, 1);
    }

    private String getTitleOnUiThread() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return getActivity().getActivityTab().getTitle();
            }
        });
    }

    /**
     * Test that a content URL is allowed within a data URL.
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlFromData() throws InterruptedException {
        final String target = "content_from_data";
        resetResourceRequestCountInContentProvider(target);
        loadUrl(UrlUtils.encodeHtmlDataUri(
                "<img src=\"" + createContentUrl(target) + "\">"));
        ensureResourceRequestCountInContentProviderNotLessThan(target, 1);
    }

    /**
     * Test that a content URL is allowed within a local file.
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testContentUrlFromFile() throws InterruptedException, IOException {
        final String target = "content_from_file";
        final File file = new File(Environment.getExternalStorageDirectory(), target + ".html");
        try {
            TestFileUtil.createNewHtmlFile(
                    file, target, "<img src=\"" + createContentUrl(target) + "\">");
            resetResourceRequestCountInContentProvider(target);
            loadUrl("file:///" + file.getAbsolutePath());
            ensureResourceRequestCountInContentProviderNotLessThan(target, 1);
        } finally {
            TestFileUtil.deleteFile(file);
        }
    }

    /**
     * Test that the browser can be navigated to a file URL.
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testFileUrlNavigation() throws InterruptedException, IOException {
        final File file = new File(Environment.getExternalStorageDirectory(),
                "url_navigation_test.html");

        try {
            TestFileUtil.createNewHtmlFile(file, "File", null);
            loadUrl("file://" + file.getAbsolutePath());
            assertEquals("File", getTitleOnUiThread());
        } finally {
            TestFileUtil.deleteFile(file);
        }
    }

    /**
     * Verifies the number of resource requests made to the content provider.
     * @param resource Resource name
     * @param expectedCount Expected resource requests count
     */
    private void ensureResourceRequestCountInContentProvider(String resource, int expectedCount) {
        Context context = getInstrumentation().getTargetContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        assertEquals(expectedCount, actualCount);
    }

    /**
     * Verifies the number of resource requests made to the content provider.
     * @param resource Resource name
     * @param expectedMinimalCount Expected minimal resource requests count
     */
    private void ensureResourceRequestCountInContentProviderNotLessThan(
            String resource, int expectedMinimalCount) {
        Context context = getInstrumentation().getTargetContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        assertTrue("Minimal expected: " + expectedMinimalCount + ", actual: " + actualCount,
                actualCount >= expectedMinimalCount);
    }

    private void resetResourceRequestCountInContentProvider(String resource) {
        Context context = getInstrumentation().getTargetContext();
        TestContentProvider.resetResourceRequestCount(context, resource);
    }

    private String createContentUrl(final String target) {
        return TestContentProvider.createContentUrl(target);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }
}
