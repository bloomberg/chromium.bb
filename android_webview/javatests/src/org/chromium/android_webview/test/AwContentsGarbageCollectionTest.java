// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.content.ContextWrapper;
import android.os.Build;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.SmallTest;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import org.chromium.android_webview.AwContents;
import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * AwContents garbage collection tests. Most apps relies on WebView being
 * garbage collected to release memory. These tests ensure that nothing
 * accidentally prevents AwContents from garbage collected, leading to leaks.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class AwContentsGarbageCollectionTest extends AwTestBase {
    private static final String TAG = "AwGcTest";

    // The system retains a strong ref to the last focused view (in InputMethodManager)
    // so allow for 1 'leaked' instance.
    private static final int MAX_IDLE_INSTANCES = 1;

    private TestDependencyFactory mOverridenFactory;

    @Override
    public void tearDown() throws Exception {
        mOverridenFactory = null;
        super.tearDown();
    }

    @Override
    protected TestDependencyFactory createTestDependencyFactory() {
        if (mOverridenFactory == null) {
            return new TestDependencyFactory();
        } else {
            return mOverridenFactory;
        }
    }

    @SuppressFBWarnings("URF_UNREAD_FIELD")
    private static class StrongRefTestContext extends ContextWrapper {
        private AwContents mAwContents;
        public void setAwContentsStrongRef(AwContents awContents) {
            mAwContents = awContents;
        }

        public StrongRefTestContext(Context context) {
            super(context);
        }
    }

    private static class GcTestDependencyFactory extends TestDependencyFactory {
        private StrongRefTestContext mContext;

        public GcTestDependencyFactory(StrongRefTestContext context) {
            mContext = context;
        }

        @Override
        public AwTestContainerView createAwTestContainerView(
                AwTestRunnerActivity activity, boolean allowHardwareAcceleration) {
            if (activity != mContext.getBaseContext()) fail();
            return new AwTestContainerView(mContext, allowHardwareAcceleration);
        }
    }

    @SuppressFBWarnings("URF_UNREAD_FIELD")
    private static class StrongRefTestAwContentsClient extends TestAwContentsClient {
        private AwContents mAwContentsStrongRef;
        public void setAwContentsStrongRef(AwContents awContents) {
            mAwContentsStrongRef = awContents;
        }
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add("v=2")
    // Enabled to obtain debug logging. Feel free to disable after first failure.
    // See https://crbug.com/544098
    public void testCreateAndGcOneTime() throws Throwable {
        Log.d(TAG, "testCreateAndGcOneTime start");
        gcAndCheckAllAwContentsDestroyed();

        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = createAwTestContainerViewOnMainSync(client);
            loadUrlAsync(containerViews[i].getAwContents(), "about:blank");
        }
        Log.d(TAG, "testCreateAndGcOneTime create views done");

        containerViews = null;
        removeAllViews();
        Log.d(TAG, "testCreateAndGcOneTime remove views done");
        gcAndCheckAllAwContentsDestroyed();
        Log.d(TAG, "testCreateAndGcOneTime end");
    }

    /*
    @SmallTest
    @Feature({"AndroidWebView"})
    Bug: https://crbug.com/544098
    */
    @FlakyTest
    public void testReferenceFromClient() throws Throwable {
        gcAndCheckAllAwContentsDestroyed();

        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            StrongRefTestAwContentsClient client = new StrongRefTestAwContentsClient();
            containerViews[i] = createAwTestContainerViewOnMainSync(client);
            loadUrlAsync(containerViews[i].getAwContents(), "about:blank");
        }

        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    /*
    @SmallTest
    @Feature({"AndroidWebView"})
    Bug: https://crbug.com/544098
    */
    @FlakyTest
    public void testReferenceFromContext() throws Throwable {
        gcAndCheckAllAwContentsDestroyed();

        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            StrongRefTestContext context = new StrongRefTestContext(getActivity());
            mOverridenFactory = new GcTestDependencyFactory(context);
            containerViews[i] = createAwTestContainerViewOnMainSync(client);
            mOverridenFactory = null;
            loadUrlAsync(containerViews[i].getAwContents(), "about:blank");
        }

        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @DisableHardwareAccelerationForTest
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testCreateAndGcManyTimes() throws Throwable {
        gcAndCheckAllAwContentsDestroyed();

        final int concurrentInstances = 4;
        final int repetitions = 16;

        for (int i = 0; i < repetitions; ++i) {
            for (int j = 0; j < concurrentInstances; ++j) {
                StrongRefTestAwContentsClient client = new StrongRefTestAwContentsClient();
                StrongRefTestContext context = new StrongRefTestContext(getActivity());
                mOverridenFactory = new GcTestDependencyFactory(context);
                AwTestContainerView view = createAwTestContainerViewOnMainSync(client);
                mOverridenFactory = null;
                // Embedding app can hold onto a strong ref to the WebView from either
                // WebViewClient or WebChromeClient. That should not prevent WebView from
                // gc-ed. We simulate that behavior by making the equivalent change here,
                // have AwContentsClient hold a strong ref to the AwContents object.
                client.setAwContentsStrongRef(view.getAwContents());
                context.setAwContentsStrongRef(view.getAwContents());
                loadUrlAsync(view.getAwContents(), "about:blank");
            }
            assertTrue(AwContents.getNativeInstanceCount() >= concurrentInstances);
            assertTrue(AwContents.getNativeInstanceCount() <= (i + 1) * concurrentInstances);
            removeAllViews();
        }

        gcAndCheckAllAwContentsDestroyed();
    }

    private void removeAllViews() throws Throwable {
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().removeAllViews();
            }
        });
    }

    private void gcAndCheckAllAwContentsDestroyed() throws InterruptedException {
        Log.d(TAG, "Cause GC");
        Runtime.getRuntime().gc();

        Criteria criteria = new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
                        @Override
                        public Boolean call() {
                            int count = AwContents.getNativeInstanceCount();
                            Log.d(TAG, "NativeInstanceCount = " + count);
                            return count <= MAX_IDLE_INSTANCES;
                        }
                    });
                } catch (Exception e) {
                    return false;
                }
            }
        };

        // Depending on a single gc call can make this test flaky. It's possible
        // that the WebView still has transient references during load so it does not get
        // gc-ed in the one gc-call above. Instead call gc again if exit criteria fails to
        // catch this case.
        final long timeoutBetweenGcMs = scaleTimeout(1000);
        for (int i = 0; i < 15; ++i) {
            if (CriteriaHelper.pollForCriteria(criteria, timeoutBetweenGcMs, CHECK_INTERVAL)) {
                break;
            } else {
                Log.d(TAG, "Cause GC");
                Runtime.getRuntime().gc();
            }
        }

        assertTrue(criteria.isSatisfied());
    }
}

