// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType.TYPE_CARD;

import android.support.v7.widget.RecyclerView;

import com.google.android.libraries.feed.api.client.stream.Stream;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryContentStorage;
import com.google.android.libraries.feed.hostimpl.storage.testing.InMemoryJournalStorage;

import org.junit.Assert;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/**
 * A test rule to inject feed card data, base on the {@DataFilePath} annotation,
 * into the FeedProcessScopeFactory via TestNetworkClient.
 * If there is no {@DataFilePath} annotation, then no data injection happens
 * for that test case.
 *
 * <pre>
 * {@code
 *
 * @RunWith(ChromeJUnit4ClassRunner.class)
 * @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
 * public class MyTest{
 *     // Provide FeedDataInjectRule with the path from src/ to the test data directory.
 *     @Rule
 *     public FeedDataInjectRule mDataInjector = new FeedDataInjectRule();
 *
 *     @Rule
 *     public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
 *         new ChromeActivityTestRule(ChromeTabbedActivity.class);
 *
 *     @Test
 *     @Feature({"FeedNewTabPage"})
 *     @DataFilePath("foo")
 *     public void testViewCard() {
 *         mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
 *         Tab tab = mActivityTestRule.getActivity().getActivityTab();
 *         NewTabPageTestUtils.waitForNtpLoaded(tab);
 *
 *         FeedNewTabPage ntp = (FeedNewTabPage) tab.getNativePage();
 *         mInjectDataRule.triggerFeedRefreshOnUiThreadBlocking(ntp.getStreamForTesting());
 *     }
 * }
 * }
 * </pre>
 *
 * Note when using this test rule, do not also use ChromeTabbedActivityTestRule.
 * ChromeTabbedActivityTestRule also try to set a default feed response file and
 * it could results to strange behaviors in test, such as always see the same
 * set of cards in each test case.
 */
final class FeedDataInjectRule extends TestWatcher {
    private static final String TAG = "FeedDataInjectRule";
    public static final int FIRST_CARD_POSITION = 3;

    private TestNetworkClient mClient;
    private boolean mTestCaseDataFileInjected;

    @Override
    protected void starting(Description desc) {
        DataFilePath filePath = desc.getAnnotation(DataFilePath.class);
        if (filePath != null) {
            mClient = new TestNetworkClient();
            try {
                mClient.setNetworkResponseFile(UrlUtils.getIsolatedTestFilePath(filePath.value()));
                FeedProcessScopeFactory.setTestNetworkClient(mClient);
                FeedProcessScopeFactory.setTestContentStorageDirect(new InMemoryContentStorage());
                FeedProcessScopeFactory.setTestJournalStorageDirect(new InMemoryJournalStorage());
                mTestCaseDataFileInjected = true;
            } catch (IOException e) {
                Log.e(TAG, "fails to set response file %s, err %s", filePath.value(),
                        e.getMessage());
                Assert.fail(
                        String.format("starting fails to set response file %s", filePath.value()));
            }
        }
    }

    @Override
    protected void finished(Description description) {
        FeedProcessScopeFactory.setTestNetworkClient(null);
        FeedProcessScopeFactory.setTestContentStorageDirect(null);
        FeedProcessScopeFactory.setTestJournalStorageDirect(null);
        mTestCaseDataFileInjected = false;
    }

    public void triggerFeedRefreshOnUiThreadBlocking(Stream stream)
            throws IllegalArgumentException, TimeoutException {
        if (stream == null) {
            throw new IllegalArgumentException("stream should not be null.");
        }
        if (mTestCaseDataFileInjected) {
            TestObserver observer =
                    new TestObserver(((RecyclerView) stream.getView()).getAdapter());
            stream.addOnContentChangedListener(observer);
            int callCount = observer.firstCardShownCallback.getCallCount();
            Log.i(TAG, "Waiting for %d callback calls", callCount);
            TestThreadUtils.runOnUiThreadBlocking(() -> stream.triggerRefresh());
            observer.firstCardShownCallback.waitForCallback(callCount);
        }
    }

    private class TestObserver implements Stream.ContentChangedListener {
        public final CallbackHelper firstCardShownCallback = new CallbackHelper();
        private final RecyclerView.Adapter mRecyclerViewAdapter;

        TestObserver(RecyclerView.Adapter adapter) {
            mRecyclerViewAdapter = adapter;
        }

        @Override
        public void onContentChanged() {
            if (mRecyclerViewAdapter.getItemViewType(FIRST_CARD_POSITION) == TYPE_CARD) {
                firstCardShownCallback.notifyCalled();
            }
        }
    }
}
