// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Context;
import android.content.ContextWrapper;
import android.os.ResultReceiver;
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwGLFunctor;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * AwContents garbage collection tests. Most apps relies on WebView being
 * garbage collected to release memory. These tests ensure that nothing
 * accidentally prevents AwContents from garbage collected, leading to leaks.
 * See crbug.com/544098 for why @DisableHardwareAccelerationForTest is needed.
 */
public class AwContentsGarbageCollectionTest extends AwTestBase {
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
        private final StrongRefTestContext mContext;

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

    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCreateAndGcOneTime() throws Throwable {
        gcAndCheckAllAwContentsDestroyed();

        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = createAwTestContainerViewOnMainSync(client);
            loadUrlAsync(containerViews[i].getAwContents(), "about:blank");
        }

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
        }
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @DisableHardwareAccelerationForTest
    @SuppressFBWarnings("UC_USELESS_OBJECT")
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHoldKeyboardResultReceiver() throws Throwable {
        gcAndCheckAllAwContentsDestroyed();

        TestAwContentsClient client = new TestAwContentsClient();
        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        ResultReceiver resultReceivers[] = new ResultReceiver[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            final AwTestContainerView containerView = createAwTestContainerViewOnMainSync(client);
            containerViews[i] = containerView;
            loadUrlAsync(containerView.getAwContents(), "about:blank");
            // When we call showSoftInput(), we pass a ResultReceiver object as a parameter.
            // Android framework will hold the object reference until the matching
            // ResultReceiver in InputMethodService (IME app) gets garbage-collected.
            // WebView object wouldn't get gc'ed once OSK shows up because of this.
            // It is difficult to show keyboard and wait until input method window shows up.
            // Instead, we simply emulate Android's behavior by keeping strong references.
            // See crbug.com/595613 for details.
            resultReceivers[i] = runTestOnUiThreadAndGetResult(new Callable<ResultReceiver>() {
                @Override
                public ResultReceiver call() throws Exception {
                    return containerView.getContentViewCore().getNewShowKeyboardReceiver();
                }
            });
        }

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
        }
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReferenceFromClient() throws Throwable {
        gcAndCheckAllAwContentsDestroyed();

        AwTestContainerView containerViews[] = new AwTestContainerView[MAX_IDLE_INSTANCES + 1];
        for (int i = 0; i < containerViews.length; i++) {
            StrongRefTestAwContentsClient client = new StrongRefTestAwContentsClient();
            containerViews[i] = createAwTestContainerViewOnMainSync(client);
            loadUrlAsync(containerViews[i].getAwContents(), "about:blank");
        }

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
        }
        containerViews = null;
        removeAllViews();
        gcAndCheckAllAwContentsDestroyed();
    }

    @DisableHardwareAccelerationForTest
    @SmallTest
    @Feature({"AndroidWebView"})
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

        for (int i = 0; i < containerViews.length; i++) {
            containerViews[i] = null;
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

    private void gcAndCheckAllAwContentsDestroyed() {
        Runtime.getRuntime().gc();

        Criteria criteria = new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
                        @Override
                        public Boolean call() {
                            int count_aw_contents = AwContents.getNativeInstanceCount();
                            int count_aw_functor = AwGLFunctor.getNativeInstanceCount();
                            return count_aw_contents <= MAX_IDLE_INSTANCES
                                    && count_aw_functor <= MAX_IDLE_INSTANCES;
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
            try {
                CriteriaHelper.pollInstrumentationThread(
                        criteria, timeoutBetweenGcMs, CHECK_INTERVAL);
                break;
            } catch (AssertionError e) {
                Runtime.getRuntime().gc();
            }
        }

        assertTrue(criteria.isSatisfied());
    }
}

