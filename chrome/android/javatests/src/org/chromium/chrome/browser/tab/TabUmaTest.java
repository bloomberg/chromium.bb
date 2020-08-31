// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.TabbedModeTabDelegateFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.channels.FileChannel;
import java.util.concurrent.ExecutionException;

/**
 * Tests for Tab-related histogram collection.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabUmaTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    @Rule
    public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private String mTestUrl;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestUrl = mTestServer.getURL(TEST_PATH);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private TabbedModeTabDelegateFactory createTabDelegateFactory() {
        BrowserControlsVisibilityDelegate visibilityDelegate =
                new BrowserControlsVisibilityDelegate(BrowserControlsState.BOTH) {};
        return new TabbedModeTabDelegateFactory(mActivityTestRule.getActivity(), visibilityDelegate,
                new ObservableSupplierImpl<ShareDelegate>(), null);
    }

    private Tab createLazilyLoadedTab(boolean show) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab bgTab = TabBuilder.createForLazyLoad(new LoadUrlParams(mTestUrl))
                                .setWindow(mActivityTestRule.getActivity().getWindowAndroid())
                                .setLaunchType(TabLaunchType.FROM_LONGPRESS_BACKGROUND)
                                .setDelegateFactory(createTabDelegateFactory())
                                .setInitiallyHidden(true)
                                .build();
            if (show) bgTab.show(TabSelectionType.FROM_USER);
            return bgTab;
        });
    }

    private Tab createLiveTab(boolean foreground, boolean kill) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = TabBuilder.createLiveTab(!foreground)
                              .setWindow(mActivityTestRule.getActivity().getWindowAndroid())
                              .setLaunchType(TabLaunchType.FROM_LONGPRESS_BACKGROUND)
                              .setDelegateFactory(createTabDelegateFactory())
                              .setInitiallyHidden(!foreground)
                              .build();
            tab.loadUrl(new LoadUrlParams(mTestUrl));

            // Simulate the renderer being killed by the OS.
            if (kill) ChromeTabUtils.simulateRendererKilledForTesting(tab, false);

            tab.show(TabSelectionType.FROM_USER);
            return tab;
        });
    }

    /**
     * Verify that Tab.StatusWhenSwitchedBackToForeground is correctly recording lazy loads.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testTabStatusWhenSwitchedToLazyLoads() throws ExecutionException {
        final Tab tab = createLazilyLoadedTab(/* show= */ false);

        String histogram = "Tab.StatusWhenSwitchedBackToForeground";
        HistogramDelta lazyLoadCount =
                new HistogramDelta(histogram, TabUma.TAB_STATUS_LAZY_LOAD_FOR_BG_TAB);
        Assert.assertEquals(0, lazyLoadCount.getDelta()); // Sanity check.

        // Show the tab and verify that one sample was recorded in the lazy load bucket.
        TestThreadUtils.runOnUiThreadBlocking(() -> { tab.show(TabSelectionType.FROM_USER); });
        Assert.assertEquals(1, lazyLoadCount.getDelta());

        // Show the tab again and verify that we didn't record another sample.
        TestThreadUtils.runOnUiThreadBlocking(() -> { tab.show(TabSelectionType.FROM_USER); });
        Assert.assertEquals(1, lazyLoadCount.getDelta());
    }

    /**
     * Verify that Uma tasks doesn't start for a Tab initialized with null creation state.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testNoCreationStateNoTabUma() throws Exception {
        String switchFgStatus = "Tab.StatusWhenSwitchedBackToForeground";

        String ageStartup = "Tabs.ForegroundTabAgeAtStartup";
        String ageRestore = "Tab.AgeUponRestoreFromColdStart";

        Assert.assertEquals(0, getHistogram(switchFgStatus));
        Assert.assertEquals(1, getHistogram(ageStartup));
        Assert.assertEquals(0, getHistogram(ageRestore));

        // Test a normal tab without an explicit creation state. UMA task doesn't start.
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return new TabBuilder()
                    .setWindow(mActivityTestRule.getActivity().getWindowAndroid())
                    .setDelegateFactory(createTabDelegateFactory())
                    .setLaunchType(TabLaunchType.FROM_LONGPRESS_BACKGROUND)
                    .setTabState(createTabState())
                    .build();
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> tab.show(TabSelectionType.FROM_USER));

        // There should be no histogram changes.
        Assert.assertEquals(0, getHistogram(switchFgStatus));
        Assert.assertEquals(1, getHistogram(ageStartup));
        Assert.assertEquals(0, getHistogram(ageRestore));
    }

    /**
     * Verify that Tab state transitions are correctly recorded.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testTabStateTransition() throws Exception {
        String transitionToInactive = "Tabs.StateTransfer.Time_Active_Inactive";
        String transitionToClosed = "Tabs.StateTransfer.Time_Active_Closed";
        String transitionTargetInitial = "Tabs.StateTransfer.Target_Initial";
        String transitionTargetActive = "Tabs.StateTransfer.Target_Active";
        String transitionTargetInactive = "Tabs.StateTransfer.Target_Inactive";

        Assert.assertEquals(1, getHistogram(transitionTargetInitial));
        Assert.assertEquals(0, getHistogram(transitionTargetInactive));
        Assert.assertEquals(0, getHistogram(transitionTargetActive));
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        createLiveTab(/* foreground= */ false, /* kill= */ false);
        Assert.assertEquals(2, getHistogram(transitionTargetInitial));
        Assert.assertEquals(1, getHistogram(transitionTargetInactive));
        Assert.assertEquals(0, getHistogram(transitionTargetActive));
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        // Test a live tab killed in background before shown.
        createLiveTab(/* foreground= */ false, /* kill= */ true);
        Assert.assertEquals(3, getHistogram(transitionTargetInitial));
        Assert.assertEquals(2, getHistogram(transitionTargetInactive));
        Assert.assertEquals(0, getHistogram(transitionTargetActive));
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        // Test a tab created in background but not loaded eagerly.
        final Tab frozenBgTab = createLazilyLoadedTab(/* show= */ true);
        Assert.assertEquals(4, getHistogram(transitionTargetInitial));
        Assert.assertEquals(3, getHistogram(transitionTargetInactive));
        Assert.assertEquals(0, getHistogram(transitionTargetActive));
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        // Test a tab whose contents was to be restored from frozen state but failed
        // at Tab#show(), so created anew.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = TabBuilder.createFromFrozenState()
                              .setWindow(mActivityTestRule.getActivity().getWindowAndroid())
                              .setDelegateFactory(createTabDelegateFactory())
                              .setTabState(createTabState())
                              .build();
            tab.show(TabSelectionType.FROM_USER);
            return tab;
        });

        Assert.assertEquals(5, getHistogram(transitionTargetInitial));
        Assert.assertEquals(4, getHistogram(transitionTargetInactive));
        Assert.assertEquals(0, getHistogram(transitionTargetActive));
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        // Test a foreground tab.
        createLiveTab(/* foreground= */ true, /* kill= */ false);
        Assert.assertEquals(6, getHistogram(transitionTargetInitial));
        Assert.assertEquals(4, getHistogram(transitionTargetInactive));
        Assert.assertEquals(0, getHistogram(transitionTargetActive));
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));
    }

    private static int getHistogram(String histogram) {
        return RecordHistogram.getHistogramTotalCountForTesting(histogram);
    }

    // Create a TabState object with random bytes of content that makes the TabState
    // restoration deliberately fail.
    private TabState createTabState() throws Exception {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            File file = mTemporaryFolder.newFile("tabStateByteBufferTestFile");
            try (FileOutputStream fileOutputStream = new FileOutputStream(file);
                    DataOutputStream dataOutputStream = new DataOutputStream(fileOutputStream)) {
                dataOutputStream.write(new byte[] {1, 2, 3});
            }

            TabState state = new TabState();
            try (FileInputStream fileInputStream = new FileInputStream(file)) {
                state.contentsState = new TabState.WebContentsState(
                        fileInputStream.getChannel().map(FileChannel.MapMode.READ_ONLY,
                                fileInputStream.getChannel().position(), file.length()));
                state.contentsState.setVersion(2);
                state.timestampMillis = 10L;
                state.parentId = 1;
                state.themeColor = 4;
                state.openerAppId = "test";
                state.tabLaunchTypeAtCreation = null;
                state.rootId = 1;
            }
            return state;
        }
    }
}
