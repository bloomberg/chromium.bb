// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.util.SparseArray;
import android.util.SparseBooleanArray;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.process_launcher.ChildProcessConnection;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.BindingManager;
import org.chromium.content.browser.ChildProcessLauncherHelper;
import org.chromium.content.browser.test.ChildProcessAllocatorSettings;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Integration tests for the BindingManager API. This test plants a mock BindingManager
 * implementation and verifies that the signals it relies on are correctly delivered.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class BindingManagerIntegrationTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static class MockBindingManager implements BindingManager {
        // Maps pid to the last received visibility state of the renderer.
        private final SparseBooleanArray mProcessInForegroundMap = new SparseBooleanArray();
        // Maps pid to a string recording calls to setInForeground() and visibilityDetermined().
        private final SparseArray<String> mVisibilityCallsMap = new SparseArray<String>();
        private boolean mIsReleaseAllModerateBindingsCalled;

        void assertIsInForeground(final int pid) {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mProcessInForegroundMap.get(pid);
                }
            });
        }

        void assertIsInBackground(final int pid) {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return !mProcessInForegroundMap.get(pid);
                }
            });
        }

        void assertSetInForegroundWasCalled(String message, final int pid) {
            CriteriaHelper.pollInstrumentationThread(new Criteria(message) {
                @Override
                public boolean isSatisfied() {
                    return mProcessInForegroundMap.indexOfKey(pid) >= 0;
                }
            });
        }

        void assertIsReleaseAllModerateBindingsCalled() {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mIsReleaseAllModerateBindingsCalled;
                }
            });
        }

        String getVisibilityCalls(int pid) {
            synchronized (mVisibilityCallsMap) {
                return mVisibilityCallsMap.get(pid);
            }
        }

        boolean isReleaseAllModerateBindingsCalled() {
            return mIsReleaseAllModerateBindingsCalled;
        }

        @Override
        public void addNewConnection(int pid, ChildProcessConnection connection) {
            synchronized (mVisibilityCallsMap) {
                mVisibilityCallsMap.put(pid, "");
            }
        }

        @Override
        public void setPriority(int pid, boolean foreground, boolean boostForPendingViews) {
            mProcessInForegroundMap.put(pid, foreground);

            synchronized (mVisibilityCallsMap) {
                if (foreground) {
                    mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "FG;");
                } else {
                    mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "BG;");
                }
            }
        }

        @Override
        public void onSentToBackground() {}

        @Override
        public void onBroughtToForeground() {}

        @Override
        public void removeConnection(int pid) {}

        @Override
        public void startModerateBindingManagement(Context context, int maxSize) {}

        @Override
        public void releaseAllModerateBindings() {
            mIsReleaseAllModerateBindingsCalled = true;
        }
    }

    private MockBindingManager mBindingManager;
    private EmbeddedTestServer mTestServer;

    private static final String FILE_PATH = "/chrome/test/data/android/test.html";
    private static final String FILE_PATH2 = "/chrome/test/data/android/simple.html";
    // about:version will always be handled by a different renderer than a local file.
    private static final String ABOUT_VERSION_PATH = "chrome://version/";
    private static final String SHARED_RENDERER_PAGE_PATH =
            "/chrome/test/data/android/bindingmanager/shared_renderer1.html";
    private static final String SHARED_RENDERER_PAGE2_PATH =
            "/chrome/test/data/android/bindingmanager/shared_renderer2.html";

    /**
     * Verifies that the .setProcessInForeground() signal is called correctly as the tabs are
     * created and switched.
     */
    @Test
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testTabSwitching() throws InterruptedException {
        // Create two tabs and wait until they are loaded, so that their renderers are around.
        final Tab[] tabs = new Tab[2];
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Foreground tab.
            TabCreator tabCreator = mActivityTestRule.getActivity().getCurrentTabCreator();
            tabs[0] = tabCreator.createNewTab(
                    new LoadUrlParams(mTestServer.getURL(FILE_PATH)),
                    TabLaunchType.FROM_CHROME_UI, null);
            // Background tab.
            tabs[1] = tabCreator.createNewTab(
                    new LoadUrlParams(mTestServer.getURL(FILE_PATH)),
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
            // On Svelte devices the background tab would not be loaded automatically, so
            // trigger the load manually.
            tabs[1].show(TabSelectionType.FROM_USER);
            tabs[1].hide();
            tabs[1].setImportance(ChildProcessImportance.NORMAL);
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], mTestServer.getURL(FILE_PATH));
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], mTestServer.getURL(FILE_PATH));

        // Wait for the new tab animations on phones to finish.
        if (!DeviceFormFactor.isTablet()) {
            final ChromeActivity activity = mActivityTestRule.getActivity();
            CriteriaHelper.pollUiThread(new Criteria("Did not finish animation") {
                @Override
                public boolean isSatisfied() {
                    Layout layout = activity.getCompositorViewHolder()
                            .getLayoutManager().getActiveLayout();
                    return !layout.isLayoutAnimating();
                }
            });
        }
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Make sure that the renderers were spawned.
            Assert.assertTrue(tabs[0].getContentViewCore().getCurrentRenderProcessId() > 0);
            Assert.assertTrue(tabs[1].getContentViewCore().getCurrentRenderProcessId() > 0);

            // Verify that the renderer of the foreground tab was signalled as visible.
            mBindingManager.assertIsInForeground(
                    tabs[0].getContentViewCore().getCurrentRenderProcessId());
            // Verify that the renderer of the tab loaded in background was signalled as not
            // visible.
            mBindingManager.assertIsInBackground(
                    tabs[1].getContentViewCore().getCurrentRenderProcessId());

            // Select tabs[1] and verify that the renderer visibility was flipped.
            TabModelUtils.setIndex(
                    mActivityTestRule.getActivity().getCurrentTabModel(), indexOf(tabs[1]));
            mBindingManager.assertIsInBackground(
                    tabs[0].getContentViewCore().getCurrentRenderProcessId());
            mBindingManager.assertIsInForeground(
                    tabs[1].getContentViewCore().getCurrentRenderProcessId());
        });
    }

    /**
     * Verifies that the .setProcessInForeground() signal is called correctly when a tab that
     * crashed in background is restored in foreground. This is a regression test for
     * http://crbug.com/399521.
     */
    @Test
    @DisabledTest(message = "crbug.com/543153")
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testCrashInBackground() throws InterruptedException {
        // Create two tabs and wait until they are loaded, so that their renderers are around.
        final Tab[] tabs = new Tab[2];
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Foreground tab.
            TabCreator tabCreator = mActivityTestRule.getActivity().getCurrentTabCreator();
            tabs[0] = tabCreator.createNewTab(
                    new LoadUrlParams(mTestServer.getURL(FILE_PATH)),
                    TabLaunchType.FROM_CHROME_UI, null);
            // Background tab.
            tabs[1] = tabCreator.createNewTab(
                    new LoadUrlParams(mTestServer.getURL(FILE_PATH)),
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
            // On Svelte devices the background tab would not be loaded automatically, so
            // trigger the load manually.
            tabs[1].show(TabSelectionType.FROM_USER);
            tabs[1].hide();
            tabs[1].setImportance(ChildProcessImportance.NORMAL);
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], mTestServer.getURL(FILE_PATH));
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], mTestServer.getURL(FILE_PATH));

        // Wait for the new tab animations on phones to finish.
        if (!DeviceFormFactor.isTablet()) {
            final ChromeActivity activity = mActivityTestRule.getActivity();
            CriteriaHelper.pollUiThread(new Criteria("Did not finish animation") {
                @Override
                public boolean isSatisfied() {
                    Layout layout = activity.getCompositorViewHolder()
                            .getLayoutManager().getActiveLayout();
                    return !layout.isLayoutAnimating();
                }
            });
        }
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Make sure that the renderers were spawned.
            Assert.assertTrue(tabs[0].getContentViewCore().getCurrentRenderProcessId() > 0);
            Assert.assertTrue(tabs[1].getContentViewCore().getCurrentRenderProcessId() > 0);

            // Verify that the renderer of the foreground tab was signalled as visible.
            mBindingManager.assertIsInForeground(
                    tabs[0].getContentViewCore().getCurrentRenderProcessId());
            // Verify that the renderer of the tab loaded in background was signalled as not
            // visible.
            mBindingManager.assertIsInBackground(
                    tabs[1].getContentViewCore().getCurrentRenderProcessId());
        });

        // Kill the renderer and wait for the crash to be noted by the browser process.
        Assert.assertTrue(ChildProcessLauncherHelper.crashProcessForTesting(
                tabs[1].getContentViewCore().getCurrentRenderProcessId()));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Renderer crash wasn't noticed by the browser.") {
                    @Override
                    public boolean isSatisfied() {
                        return tabs[1].getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                });

        // Switch to the tab that crashed in background.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> TabModelUtils.setIndex(
                mActivityTestRule.getActivity().getCurrentTabModel(), indexOf(tabs[1])));

        // Wait until the process is spawned and its visibility is determined.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Process for the crashed tab was not respawned.") {
                    @Override
                    public boolean isSatisfied() {
                        return tabs[1].getContentViewCore().getCurrentRenderProcessId() != 0;
                    }
                });

        mBindingManager.assertSetInForegroundWasCalled(
                "isInForeground() was not called for the process.",
                tabs[1].getContentViewCore().getCurrentRenderProcessId());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Verify the visibility of the renderers.
            mBindingManager.assertIsInBackground(
                    tabs[0].getContentViewCore().getCurrentRenderProcessId());
            mBindingManager.assertIsInForeground(
                    tabs[1].getContentViewCore().getCurrentRenderProcessId());
        });
    }

    /**
     * Verifies that a renderer that crashes in foreground has the correct visibility when
     * recreated.
     */
    @Test
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testCrashInForeground() throws InterruptedException {
        // Create a tab in foreground and wait until it is loaded.
        final String testUrl = mTestServer.getURL(FILE_PATH);
        final Tab tab = ThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    TabCreator tabCreator =
                            mActivityTestRule.getActivity().getCurrentTabCreator();
                    return tabCreator.createNewTab(
                            new LoadUrlParams(testUrl), TabLaunchType.FROM_CHROME_UI, null);
                });
        ChromeTabUtils.waitForTabPageLoaded(tab, testUrl);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Kill the renderer and wait for the crash to be noted by the browser process.
        Assert.assertTrue(ChildProcessLauncherHelper.crashProcessForTesting(
                tab.getContentViewCore().getCurrentRenderProcessId()));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Renderer crash wasn't noticed by the browser.") {
                    @Override
                    public boolean isSatisfied() {
                        return tab.getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                });

        // Reload the tab, respawning the renderer.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> tab.reload());

        ChromeTabUtils.waitForTabPageLoaded(tab, testUrl);

        // Wait until the process is spawned and its visibility is determined.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Process for the crashed tab was not respawned.") {
                    @Override
                    public boolean isSatisfied() {
                        return tab.getContentViewCore().getCurrentRenderProcessId() != 0;
                    }
                });

        mBindingManager.assertSetInForegroundWasCalled(
                "isInForeground() was not called for the process.",
                tab.getContentViewCore().getCurrentRenderProcessId());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Verify the visibility of the renderer.
            mBindingManager.assertIsInForeground(
                    tab.getContentViewCore().getCurrentRenderProcessId());
        });
    }

    private int getRenderProcessId(final Tab tab) {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> tab.getContentViewCore().getCurrentRenderProcessId());
    }

    /**
     * Verifies that BindingManager.releaseAllModerateBindings() is called once all the sandboxed
     * services are allocated.
     */
    @Test
    @ChildProcessAllocatorSettings(sandboxedServiceCount = 4)
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testReleaseAllModerateBindings() throws InterruptedException {
        final TabCreator tabCreator = mActivityTestRule.getActivity().getCurrentTabCreator();
        final Tab[] tabs = new Tab[3];
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Foreground tab.
            tabs[0] = tabCreator.createNewTab(
                    new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
            // Background tab.
            tabs[1] = tabCreator.createNewTab(
                    new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], "about:blank");
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], "about:blank");
        // At this point 3 sandboxed services are allocated; the initial one + 2 new tabs.
        Assert.assertFalse(mBindingManager.isReleaseAllModerateBindingsCalled());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Foreground tab.
            tabs[2] = tabCreator.createNewTab(
                    new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[2], "about:blank");
        // At this point all the sandboxed services are allocated.
        mBindingManager.assertIsReleaseAllModerateBindingsCalled();
    }

    @Test
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testRestoreSharedRenderer() throws Exception {
        mActivityTestRule.loadUrl(mTestServer.getURL(SHARED_RENDERER_PAGE_PATH));

        final Tab[] tabs = new Tab[2];
        tabs[0] = mActivityTestRule.getActivity().getActivityTab();
        TouchCommon.singleClickView(tabs[0].getView());

        CriteriaHelper.pollInstrumentationThread(new Criteria("Child tab isn't opened.") {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.getActivity().getCurrentTabModel().getCount() == 2
                        && tabs[0] != mActivityTestRule.getActivity().getActivityTab()
                        && mActivityTestRule.getActivity()
                                   .getActivityTab()
                                   .getContentViewCore()
                                   .getCurrentRenderProcessId()
                        != 0;
            }
        });
        tabs[1] = mActivityTestRule.getActivity().getActivityTab();
        Assert.assertEquals(tabs[0].getContentViewCore().getCurrentRenderProcessId(),
                tabs[1].getContentViewCore().getCurrentRenderProcessId());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Verify the visibility of the renderer.
            mBindingManager.assertIsInForeground(
                    tabs[0].getContentViewCore().getCurrentRenderProcessId());
        });

        Assert.assertTrue(ChildProcessLauncherHelper.crashProcessForTesting(
                tabs[1].getContentViewCore().getCurrentRenderProcessId()));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Renderer crash wasn't noticed by the browser.") {
                    @Override
                    public boolean isSatisfied() {
                        return tabs[1].getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                });
        // Reload the tab, respawning the renderer.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> tabs[1].reload());

        ChromeTabUtils.waitForTabPageLoaded(
                tabs[1], mTestServer.getURL(SHARED_RENDERER_PAGE2_PATH));

        // Wait until the process is spawned and its visibility is determined.
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Process for the crashed tab was not respawned.") {
                    @Override
                    public boolean isSatisfied() {
                        return tabs[1].getContentViewCore().getCurrentRenderProcessId() != 0;
                    }
                });

        mBindingManager.assertSetInForegroundWasCalled(
                "setInForeground() was not called for the process.",
                tabs[1].getContentViewCore().getCurrentRenderProcessId());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Verify the visibility of the renderer.
            mBindingManager.assertIsInForeground(
                    tabs[1].getContentViewCore().getCurrentRenderProcessId());
            tabs[1].hide();
            tabs[1].setImportance(ChildProcessImportance.NORMAL);
            mBindingManager.assertIsInBackground(
                    tabs[1].getContentViewCore().getCurrentRenderProcessId());
        });
    }

    @Before
    public void setUp() throws Exception {
        // Hook in the test binding manager.
        mBindingManager = new MockBindingManager();
        ChildProcessLauncherHelper.setBindingManagerForTesting(mBindingManager);

        mActivityTestRule.startMainActivityOnBlankPage();

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * @return the index of the given tab in the current tab model
     */
    private int indexOf(Tab tab) {
        return mActivityTestRule.getActivity().getCurrentTabModel().indexOf(tab);
    }
}
