// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.net.ConnectionType;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import javax.annotation.Nullable;

/**
 * Test for {@link FeedbackCollector}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class FeedbackCollectorTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final int CONNECTIVITY_TASK_TIMEOUT_MS = 10;

    private ChromeActivity mActivity;
    private Profile mProfile;
    private TestFeedbackCollector mCollector;
    private TestConnectivityTask mTestConnectivityTask;

    /**
     * Class for facilitating testing of {@link FeedbackCollector}. All public methods are
     * automatically run on the UI thread, to simplify testing code.
     *
     * In addition, the {@link FeedbackCollector#init} method is overridden to ensure
     * no real tasks are started during creation.
     */
    class TestFeedbackCollector extends FeedbackCollector {
        private final AtomicBoolean mTimedOut = new AtomicBoolean(false);

        TestFeedbackCollector(Activity activity, Profile profile, String url,
                boolean takeScreenshot, FeedbackResult callback) {
            super(activity, profile, url, takeScreenshot, callback);
        }

        @Override
        void init(Activity activity) {
            mTestConnectivityTask =
                    new TestConnectivityTask(mProfile, CONNECTIVITY_TASK_TIMEOUT_MS, null);
            mConnectivityTask = mTestConnectivityTask;
        }

        @Override
        public void onResult(final ConnectivityTask.FeedbackData feedbackData) {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    TestFeedbackCollector.super.onResult(feedbackData);
                }
            });
        }

        @Override
        public void onGotBitmap(@Nullable final Bitmap bitmap) {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    TestFeedbackCollector.super.onGotBitmap(bitmap);
                }
            });
        }

        @Override
        void maybePostResult() {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    TestFeedbackCollector.super.maybePostResult();
                }
            });
        }

        @Override
        public void add(final String key, final String value) {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    TestFeedbackCollector.super.add(key, value);
                }
            });
        }

        @Override
        public void setDescription(final String description) {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    TestFeedbackCollector.super.setDescription(description);
                }
            });
        }

        @Override
        public String getDescription() {
            return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
                @Override
                public String call() {
                    return TestFeedbackCollector.super.getDescription();
                }
            });
        }

        @Override
        public void setScreenshot(final Bitmap screenshot) {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    TestFeedbackCollector.super.setScreenshot(screenshot);
                }
            });
        }

        @Override
        public Bitmap getScreenshot() {
            return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Bitmap>() {
                @Override
                public Bitmap call() {
                    return TestFeedbackCollector.super.getScreenshot();
                }
            });
        }

        @Override
        public Bundle getBundle() {
            return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Bundle>() {
                @Override
                public Bundle call() {
                    return TestFeedbackCollector.super.getBundle();
                }
            });
        }

        @Override
        boolean hasTimedOut() {
            return mTimedOut.get();
        }

        void setTimedOut(boolean timedOut) {
            mTimedOut.set(timedOut);
        }
    }

    static class TestConnectivityTask extends ConnectivityTask {
        private final AtomicReference<FeedbackData> mFeedbackData = new AtomicReference<>();

        TestConnectivityTask(Profile profile, int timeoutMs, ConnectivityResult callback) {
            super(profile, timeoutMs, callback);
        }

        @Override
        void init(Profile profile, int timeoutMs) {
            super.init(profile, timeoutMs);
        }

        @Override
        public FeedbackData get() {
            return mFeedbackData.get();
        }

        void setFeedbackData(FeedbackData feedbackData) {
            mFeedbackData.set(feedbackData);
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mProfile = Profile.getLastUsedProfile();
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testGatheringOfData() {
        mCollector = createCollector("http://www.example.com/", true /* takeScreenshot */, null);
        ConnectivityTask.FeedbackData feedbackData = createFeedbackData();
        mTestConnectivityTask.setFeedbackData(feedbackData);
        mCollector.setDescription("some description");
        mCollector.add("foo", "bar");
        Bitmap bitmap = createBitmap();
        mCollector.setScreenshot(bitmap);

        Bundle bundle = mCollector.getBundle();
        Assert.assertEquals("http://www.example.com/", bundle.getString(FeedbackCollector.URL_KEY));
        Assert.assertEquals("CONNECTED", bundle.getString(ConnectivityTask.CHROME_HTTPS_KEY));
        Assert.assertEquals("some description", mCollector.getDescription());
        Assert.assertEquals("bar", bundle.getString("foo"));
        Assert.assertEquals(bitmap, mCollector.getScreenshot());
        Assert.assertEquals("false",
                bundle.getString(DataReductionProxySettings.DATA_REDUCTION_PROXY_ENABLED_KEY));
        Assert.assertEquals("Disabled", bundle.getString(FeedbackCollector.CHROME_HOME_STATE_KEY));
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testGatheringOfDataWithCallback() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicBoolean hasResult = new AtomicBoolean(false);
        FeedbackCollector.FeedbackResult callback = new FeedbackCollector.FeedbackResult() {
            @Override
            public void onResult(FeedbackCollector collector) {
                hasResult.set(true);
                semaphore.release();
            }
        };
        mCollector =
                createCollector("http://www.example.com/", true /* takeScreenshot */, callback);
        Assert.assertFalse("Result should not be ready directly after creation.", hasResult.get());
        ConnectivityTask.FeedbackData feedbackData = createFeedbackData();
        mCollector.onResult(feedbackData);
        Assert.assertFalse("Result should not be ready after connectivity data.", hasResult.get());
        mCollector.setDescription("some description");
        mCollector.add("foo", "bar");
        Bitmap bitmap = createBitmap();
        mCollector.onGotBitmap(bitmap);

        // Wait until the callback has been called.
        Assert.assertTrue(
                "Failed to acquire semaphore.", semaphore.tryAcquire(1, TimeUnit.SECONDS));
        Assert.assertTrue("Result should be ready after retrieving all data.", hasResult.get());

        Bundle bundle = mCollector.getBundle();
        Assert.assertEquals("http://www.example.com/", bundle.getString(FeedbackCollector.URL_KEY));
        Assert.assertEquals("CONNECTED", bundle.getString(ConnectivityTask.CHROME_HTTPS_KEY));
        Assert.assertEquals("some description", mCollector.getDescription());
        Assert.assertEquals("bar", bundle.getString("foo"));
        Assert.assertEquals(bitmap, mCollector.getScreenshot());
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testGatheringOfDataTimesOut() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicBoolean hasResult = new AtomicBoolean(false);
        FeedbackCollector.FeedbackResult callback = new FeedbackCollector.FeedbackResult() {
            @Override
            public void onResult(FeedbackCollector collector) {
                hasResult.set(true);
                semaphore.release();
            }
        };
        mCollector = createCollector(null, true /* takeScreenshot */, callback);
        Assert.assertFalse("Result should not be ready directly after creation.", hasResult.get());
        ConnectivityTask.FeedbackData feedbackData = createFeedbackData();
        // Set the feedback data on the connectivity task instead of through callback.
        mTestConnectivityTask.setFeedbackData(feedbackData);
        Assert.assertFalse("Result should not be ready after connectivity data.", hasResult.get());
        Bitmap bitmap = createBitmap();
        mCollector.onGotBitmap(bitmap);

        // This timeout task should trigger the callback.
        mCollector.setTimedOut(true);
        mCollector.maybePostResult();
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        // Wait until the callback has been called.
        Assert.assertTrue(
                "Failed to acquire semaphore.", semaphore.tryAcquire(1, TimeUnit.SECONDS));
        Assert.assertTrue("Result should be ready after retrieving all data.", hasResult.get());

        Bundle bundle = mCollector.getBundle();
        Assert.assertEquals("CONNECTED", bundle.getString(ConnectivityTask.CHROME_HTTPS_KEY));
        Assert.assertEquals(bitmap, mCollector.getScreenshot());
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testGatheringOfDataWaitForScreenshot() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicBoolean hasResult = new AtomicBoolean(false);
        FeedbackCollector.FeedbackResult callback = new FeedbackCollector.FeedbackResult() {
            @Override
            public void onResult(FeedbackCollector collector) {
                hasResult.set(true);
                semaphore.release();
            }
        };
        mCollector = createCollector(null, true /* takeScreenshot */, callback);
        Assert.assertFalse("Result should not be ready directly after creation.", hasResult.get());
        ConnectivityTask.FeedbackData feedbackData = createFeedbackData();
        mCollector.onResult(feedbackData);
        Assert.assertFalse("Result should not be ready after connectivity data.", hasResult.get());

        // This timeout task should not trigger the callback.
        mCollector.setTimedOut(true);
        mCollector.maybePostResult();
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        Assert.assertFalse("Result should not be ready after timeout.", hasResult.get());

        // Trigger callback by finishing taking the screenshot.
        Bitmap bitmap = createBitmap();
        mCollector.onGotBitmap(bitmap);

        // Wait until the callback has been called.
        Assert.assertTrue(
                "Failed to acquire semaphore.", semaphore.tryAcquire(1, TimeUnit.SECONDS));
        Assert.assertTrue("Result should be ready after retrieving all data.", hasResult.get());

        Bundle bundle = mCollector.getBundle();
        // The FeedbackData should have been gathered from the ConnectivityTask directly.
        Assert.assertEquals("CONNECTED", bundle.getString(ConnectivityTask.CHROME_HTTPS_KEY));
        Assert.assertEquals(bitmap, mCollector.getScreenshot());
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testGatheringOfDataNoScreenshot() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicBoolean hasResult = new AtomicBoolean(false);
        FeedbackCollector.FeedbackResult callback = new FeedbackCollector.FeedbackResult() {
            @Override
            public void onResult(FeedbackCollector collector) {
                hasResult.set(true);
                semaphore.release();
            }
        };
        mCollector = createCollector(null, false /* takeScreenshot */, callback);
        Assert.assertFalse("Result should not be ready directly after creation.", hasResult.get());
        ConnectivityTask.FeedbackData feedbackData = createFeedbackData();
        mCollector.onResult(feedbackData);
        // The result from the connectivity task should trigger the callback since we shouldn't be
        // waiting for the screenshot.
        Assert.assertTrue(
                "Failed to acquire semaphore.", semaphore.tryAcquire(5, TimeUnit.SECONDS));
        Assert.assertTrue("Result should be ready after retrieving all data.", hasResult.get());

        Bundle bundle = mCollector.getBundle();
        // The FeedbackData should have been gathered from the ConnectivityTask directly.
        Assert.assertEquals("CONNECTED", bundle.getString(ConnectivityTask.CHROME_HTTPS_KEY));
    }

    private static ConnectivityTask.FeedbackData createFeedbackData() {
        Map<ConnectivityTask.Type, Integer> connections = new HashMap<>();
        connections.put(ConnectivityTask.Type.CHROME_HTTPS, ConnectivityCheckResult.CONNECTED);
        return new ConnectivityTask.FeedbackData(connections, 10, 10, ConnectionType.CONNECTION_3G);
    }

    private static Bitmap createBitmap() {
        return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_4444);
    }

    private TestFeedbackCollector createCollector(final String url, final boolean takeScreenshot,
            final FeedbackCollector.FeedbackResult callback) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<TestFeedbackCollector>() {
            @Override
            public TestFeedbackCollector call() {
                return new TestFeedbackCollector(
                        mActivity, mProfile, url, takeScreenshot, callback);
            }
        });
    }
}
