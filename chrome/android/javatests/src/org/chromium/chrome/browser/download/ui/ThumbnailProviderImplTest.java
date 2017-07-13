// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.graphics.Bitmap;
import android.support.test.filters.MediumTest;

import junit.framework.Assert;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.download.ui.ThumbnailProvider.ThumbnailRequest;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Instrumentation test for {@link ThumbnailProviderImpl}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class ThumbnailProviderImplTest {
    private static final String TEST_DIRECTORY = "chrome/test/data/android/thumbnail_provider/";
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ThumbnailProviderImpl mThumbnailProvider;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mThumbnailProvider = new ThumbnailProviderImpl();
        clearThumbnailCache();
    }

    @After
    public void tearDown() {
        clearThumbnailCache();
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testBothThumbnailSidesSmallerThanRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_10x10.jpg");
        final int requiredSize = 20;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request = new TestThumbnailRequest(
                testFilePath, requiredSize, thumbnailRetrievedCallbackHelper);

        ThreadUtils.runOnUiThread(new Runnable() {
            public void run() {
                mThumbnailProvider.getThumbnail(request);
            }
        });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(10, 10, request);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testBothThumbnailSidesEqualToRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_10x10.jpg");
        final int requiredSize = 10;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request = new TestThumbnailRequest(
                testFilePath, requiredSize, thumbnailRetrievedCallbackHelper);

        ThreadUtils.runOnUiThread(new Runnable() {
            public void run() {
                mThumbnailProvider.getThumbnail(request);
            }
        });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(requiredSize, requiredSize, request);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testBothThumbnailSidesBiggerThanRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_20x20.jpg");
        final int requiredSize = 10;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request = new TestThumbnailRequest(
                testFilePath, requiredSize, thumbnailRetrievedCallbackHelper);

        ThreadUtils.runOnUiThread(new Runnable() {
            public void run() {
                mThumbnailProvider.getThumbnail(request);
            }
        });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(requiredSize, requiredSize, request);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testThumbnailWidthEqualToRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_10x20.jpg");
        final int requiredSize = 10;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request = new TestThumbnailRequest(
                testFilePath, requiredSize, thumbnailRetrievedCallbackHelper);

        ThreadUtils.runOnUiThread(new Runnable() {
            public void run() {
                mThumbnailProvider.getThumbnail(request);
            }
        });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(requiredSize, 20, request);
    }

    @Test
    @MediumTest
    @Feature({"Suggestions"})
    public void testThumbnailHeightEqualToRequiredSize() throws Exception {
        final String testFilePath =
                UrlUtils.getIsolatedTestFilePath(TEST_DIRECTORY + "test_image_20x10.jpg");
        final int requiredSize = 10;

        CallbackHelper thumbnailRetrievedCallbackHelper = new CallbackHelper();
        final TestThumbnailRequest request = new TestThumbnailRequest(
                testFilePath, requiredSize, thumbnailRetrievedCallbackHelper);

        ThreadUtils.runOnUiThread(new Runnable() {
            public void run() {
                mThumbnailProvider.getThumbnail(request);
            }
        });

        thumbnailRetrievedCallbackHelper.waitForCallback(
                "Reached timeout when fetching a thumbnail for a downloaded image.", 0);

        checkResult(20, requiredSize, request);
    }

    private void checkResult(
            int expectedWidth, int expectedHeight, final TestThumbnailRequest request) {
        Assert.assertEquals(expectedWidth, request.getRetrievedThumbnail().getWidth());
        Assert.assertEquals(expectedHeight, request.getRetrievedThumbnail().getHeight());
    }

    private void clearThumbnailCache() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ThumbnailProviderImpl.clearCache();
            }
        });
    }

    private static class TestThumbnailRequest implements ThumbnailRequest {
        private final String mTestFilePath;
        private final int mRequiredSize;
        private Bitmap mRetrievedThumbnail;
        private CallbackHelper mThumbnailRetrievedCallbackHelper;

        TestThumbnailRequest(String filepath, int requiredSize,
                CallbackHelper thumbnailRetrievedCallbackHelperHelper) {
            mTestFilePath = filepath;
            mRequiredSize = requiredSize;
            mThumbnailRetrievedCallbackHelper = thumbnailRetrievedCallbackHelperHelper;
        }

        @Override
        public String getFilePath() {
            return mTestFilePath;
        }

        @Override
        public void onThumbnailRetrieved(String filePath, Bitmap thumbnail) {
            Assert.assertEquals(mTestFilePath, filePath);
            mRetrievedThumbnail = thumbnail;
            mThumbnailRetrievedCallbackHelper.notifyCalled();
        }

        @Override
        public int getIconSize() {
            return mRequiredSize;
        }

        Bitmap getRetrievedThumbnail() {
            return mRetrievedThumbnail;
        }
    }
}
