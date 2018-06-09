// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.net.ConnectivityManager;
import android.net.Uri;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.download.internal.NetworkStatusListenerAndroid;
import org.chromium.net.NetworkChangeNotifierAutoDetect;
import org.chromium.net.test.util.WebServer;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Instrumentation tests for Prefetch.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrefetchFlowTest implements WebServer.RequestHandler {
    static final String TAG = "PrefetchFlowTest";
    private TestOfflinePageService mOPS = new TestOfflinePageService();
    private WebServer mServer;
    private Profile mProfile;
    private CallbackHelper mPageAddedHelper = new CallbackHelper();
    private List<OfflinePageItem> mAddedPages =
            Collections.synchronizedList(new ArrayList<OfflinePageItem>());

    // A fake NetworkChangeNotifierAutoDetect which always reports a connection.
    private static class FakeAutoDetect extends NetworkChangeNotifierAutoDetect {
        public FakeAutoDetect(Observer observer, RegistrationPolicy policy) {
            super(observer, policy);
        }
        @Override
        public NetworkState getCurrentNetworkState() {
            return new NetworkState(true, ConnectivityManager.TYPE_WIFI, 0, null);
        }
    }

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        // Start the server before Chrome starts, we use the server address in flags.
        mServer = new WebServer(0, false);
        mServer.setRequestHandler(this);
        // Inject FakeAutoDetect so that the download component will attempt to download a file even
        // when there is no connection.
        NetworkStatusListenerAndroid.setAutoDetectFactory(
                new NetworkStatusListenerAndroid.AutoDetectFactory() {
                    @Override
                    public NetworkChangeNotifierAutoDetect create(
                            NetworkChangeNotifierAutoDetect.Observer observer,
                            NetworkChangeNotifierAutoDetect.RegistrationPolicy policy) {
                        return new FakeAutoDetect(observer, policy);
                    }
                });

        // Configure flags:
        // - Enable OfflinePagesPrefetching
        // - Set DownloadService's start_up_delay_ms to 100 ms (5 seconds is default).
        // - Set offline_pages_backend.
        final String offlinePagesBackend = Uri.encode(mServer.getBaseUrl());
        CommandLine.getInstance().appendSwitchWithValue("enable-features",
                "OfflinePagesPrefetching<Trial,DownloadService<Trial,NTPArticleSuggestions<Trial");
        CommandLine.getInstance().appendSwitchWithValue("force-fieldtrials", "Trial/Group");
        CommandLine.getInstance().appendSwitchWithValue("force-fieldtrial-params",
                "Trial.Group:start_up_delay_ms/100/offline_pages_backend/" + offlinePagesBackend);

        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mProfile = mActivityTestRule.getActivity().getActivityTab().getProfile();
            OfflinePageBridge.getForProfile(mProfile).addObserver(
                    new OfflinePageBridge.OfflinePageModelObserver() {
                        @Override
                        public void offlinePageAdded(OfflinePageItem addedPage) {
                            mAddedPages.add(addedPage);
                            mPageAddedHelper.notifyCalled();
                        }
                    });
            PrefetchTestBridge.enableLimitlessPrefetching(true);
        });
    }

    @After
    public void tearDown() throws Exception {
        mServer.shutdown();
    }

    @Override
    public void handleRequest(WebServer.HTTPRequest request, OutputStream stream) {
        try {
            mOPS.handleRequest(request, stream);
        } catch (IOException e) {
            Assert.fail(e.getMessage());
        }
    }

    private void runAndWaitForBackgroundTask() throws Throwable {
        CallbackHelper finished = new CallbackHelper();
        PrefetchBackgroundTask task = new PrefetchBackgroundTask();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            TaskParameters.Builder builder =
                    TaskParameters.create(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
            PrefetchBackgroundTask.skipConditionCheckingForTesting();
            task.onStartTask(ContextUtils.getApplicationContext(), builder.build(),
                    (boolean needsReschedule) -> { finished.notifyCalled(); });
        });
        finished.waitForCallback(0);
    }

    @Test
    @MediumTest
    @Feature({"OfflinePrefetch"})
    public void testPrefetchPage() throws Throwable {
        // TODO(crbug.com/845310): Expand this test. There's some important flows missing and
        // systems missing:
        // - Zine suggestions & thumbnails
        // - GCM & GetOperation
        // - Pages that don't render
        final String url = "http://www.mysuggestion.com/";

        ThreadUtils.runOnUiThreadBlocking(
                () -> { PrefetchTestBridge.addSuggestedURLs(mProfile, new String[] {url}); });
        runAndWaitForBackgroundTask();
        mPageAddedHelper.waitForCallback(0);
        Assert.assertEquals(url, mAddedPages.get(0).getUrl());
        Assert.assertEquals(TestOfflinePageService.BODY_LENGTH, mAddedPages.get(0).getFileSize());
        Assert.assertNotEquals(0, mAddedPages.get(0).getFilePath().length());
    }
}
