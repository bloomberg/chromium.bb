// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.shell.ChromeShellActivity;
import org.chromium.chrome.shell.ChromeShellTab;
import org.chromium.chrome.shell.ChromeShellTestBase;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test for {@link FeedbackCollector}.
 */
public class FeedbackCollectorTest extends ChromeShellTestBase {
    private static final int CONNECTIVITY_TASK_TIMEOUT_MS = 10;

    private ChromeShellActivity mActivity;
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
        TestFeedbackCollector(Profile profile, String url) {
            super(profile, url);
        }

        @Override
        void init() {
            mTestConnectivityTask =
                    new TestConnectivityTask(mProfile, CONNECTIVITY_TASK_TIMEOUT_MS, null);
            mConnectivityTask = mTestConnectivityTask;
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

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mActivity = launchChromeShellWithBlankPage();
        assertTrue(waitForActiveShellToBeDoneLoading());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ChromeShellTab tab = mActivity.getActiveTab();
                mProfile = tab.getProfile();
            }
        });
    }

    @SmallTest
    @Feature({"Feedback"})
    public void testGatheringOfData() {
        mCollector = createCollector("http://www.example.com/");
        ConnectivityTask.FeedbackData feedbackData = createFeedbackData();
        mTestConnectivityTask.setFeedbackData(feedbackData);
        mCollector.setDescription("some description");
        mCollector.add("foo", "bar");
        Bitmap bitmap = createBitmap();
        mCollector.setScreenshot(bitmap);

        Bundle bundle = mCollector.getBundle();
        assertEquals("http://www.example.com/", bundle.getString(FeedbackCollector.URL_KEY));
        assertEquals("CONNECTED", bundle.getString(ConnectivityTask.CHROME_HTTPS_KEY));
        assertEquals("some description", mCollector.getDescription());
        assertEquals("bar", bundle.getString("foo"));
        assertEquals(bitmap, mCollector.getScreenshot());
    }

    private static ConnectivityTask.FeedbackData createFeedbackData() {
        Map<ConnectivityTask.Type, Integer> connections = new HashMap<>();
        connections.put(ConnectivityTask.Type.CHROME_HTTPS, ConnectivityCheckResult.CONNECTED);
        return new ConnectivityTask.FeedbackData(connections, 10, 10);
    }

    private static Bitmap createBitmap() {
        return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_4444);
    }

    private TestFeedbackCollector createCollector(final String url) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<TestFeedbackCollector>() {
            @Override
            public TestFeedbackCollector call() {
                return new TestFeedbackCollector(mProfile, url);
            }
        });
    }
}
