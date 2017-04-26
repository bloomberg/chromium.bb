// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.support.test.filters.LargeTest;
import android.test.MoreAsserts;
import android.util.SparseArray;
import android.util.SparseBooleanArray;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.PrerenderTestHelper;
import org.chromium.content.browser.BindingManager;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.browser.ManagedChildProcessConnection;
import org.chromium.content.browser.test.ChildProcessAllocatorSettings;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.Callable;

/**
 * Integration tests for the BindingManager API. This test plants a mock BindingManager
 * implementation and verifies that the signals it relies on are correctly delivered.
 */
@RetryOnFailure
public class BindingManagerIntegrationTest extends ChromeActivityTestCaseBase<ChromeActivity> {

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
        public void addNewConnection(int pid, ManagedChildProcessConnection connection) {
            synchronized (mVisibilityCallsMap) {
                mVisibilityCallsMap.put(pid, "");
            }
        }

        @Override
        public void setInForeground(int pid, boolean inForeground) {
            mProcessInForegroundMap.put(pid, inForeground);

            synchronized (mVisibilityCallsMap) {
                if (inForeground) {
                    mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "FG;");
                } else {
                    mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "BG;");
                }
            }
        }

        @Override
        public void onDeterminedVisibility(int pid) {
            synchronized (mVisibilityCallsMap) {
                mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "DETERMINED;");
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

    public BindingManagerIntegrationTest() {
        super(ChromeActivity.class);
    }

    /**
     * Verifies that the .setProcessInForeground() signal is called correctly as the tabs are
     * created and switched.
     */
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testTabSwitching() throws InterruptedException {
        // Create two tabs and wait until they are loaded, so that their renderers are around.
        final Tab[] tabs = new Tab[2];
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                TabCreator tabCreator = getActivity().getCurrentTabCreator();
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
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], mTestServer.getURL(FILE_PATH));
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], mTestServer.getURL(FILE_PATH));

        // Wait for the new tab animations on phones to finish.
        if (!DeviceFormFactor.isTablet(getActivity())) {
            final ChromeActivity activity = getActivity();
            CriteriaHelper.pollUiThread(new Criteria("Did not finish animation") {
                @Override
                public boolean isSatisfied() {
                    Layout layout = activity.getCompositorViewHolder()
                            .getLayoutManager().getActiveLayout();
                    return !layout.isLayoutAnimating();
                }
            });
        }
        getInstrumentation().waitForIdleSync();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Make sure that the renderers were spawned.
                assertTrue(tabs[0].getContentViewCore().getCurrentRenderProcessId() > 0);
                assertTrue(tabs[1].getContentViewCore().getCurrentRenderProcessId() > 0);

                // Verify that the renderer of the foreground tab was signalled as visible.
                mBindingManager.assertIsInForeground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId());
                // Verify that the renderer of the tab loaded in background was signalled as not
                // visible.
                mBindingManager.assertIsInBackground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId());

                // Select tabs[1] and verify that the renderer visibility was flipped.
                TabModelUtils.setIndex(getActivity().getCurrentTabModel(), indexOf(tabs[1]));
                mBindingManager.assertIsInBackground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId());
                mBindingManager.assertIsInForeground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId());
            }
        });
    }

    /**
     * Verifies that the .setProcessInForeground() signal is called correctly when a tab that
     * crashed in background is restored in foreground. This is a regression test for
     * http://crbug.com/399521.
     */
    @DisabledTest(message = "crbug.com/543153")
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testCrashInBackground() throws InterruptedException {
        // Create two tabs and wait until they are loaded, so that their renderers are around.
        final Tab[] tabs = new Tab[2];
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                TabCreator tabCreator = getActivity().getCurrentTabCreator();
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
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], mTestServer.getURL(FILE_PATH));
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], mTestServer.getURL(FILE_PATH));

        // Wait for the new tab animations on phones to finish.
        if (!DeviceFormFactor.isTablet(getActivity())) {
            final ChromeActivity activity = getActivity();
            CriteriaHelper.pollUiThread(new Criteria("Did not finish animation") {
                @Override
                public boolean isSatisfied() {
                    Layout layout = activity.getCompositorViewHolder()
                            .getLayoutManager().getActiveLayout();
                    return !layout.isLayoutAnimating();
                }
            });
        }
        getInstrumentation().waitForIdleSync();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Make sure that the renderers were spawned.
                assertTrue(tabs[0].getContentViewCore().getCurrentRenderProcessId() > 0);
                assertTrue(tabs[1].getContentViewCore().getCurrentRenderProcessId() > 0);

                // Verify that the renderer of the foreground tab was signalled as visible.
                mBindingManager.assertIsInForeground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId());
                // Verify that the renderer of the tab loaded in background was signalled as not
                // visible.
                mBindingManager.assertIsInBackground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId());
            }
        });

        // Kill the renderer and wait for the crash to be noted by the browser process.
        assertTrue(ChildProcessLauncher.crashProcessForTesting(
                tabs[1].getContentViewCore().getCurrentRenderProcessId()));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Renderer crash wasn't noticed by the browser.") {
                    @Override
                    public boolean isSatisfied() {
                        return tabs[1].getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                });

        // Switch to the tab that crashed in background.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(getActivity().getCurrentTabModel(), indexOf(tabs[1]));
            }
        });

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

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Verify the visibility of the renderers.
                mBindingManager.assertIsInBackground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId());
                mBindingManager.assertIsInForeground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId());
            }
        });
    }

    /**
     * Verifies that a renderer that crashes in foreground has the correct visibility when
     * recreated.
     */
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testCrashInForeground() throws InterruptedException {
        // Create a tab in foreground and wait until it is loaded.
        final String testUrl = mTestServer.getURL(FILE_PATH);
        final Tab tab = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Tab>() {
                    @Override
                    public Tab call() throws Exception {
                        TabCreator tabCreator = getActivity().getCurrentTabCreator();
                        return tabCreator.createNewTab(
                                new LoadUrlParams(testUrl), TabLaunchType.FROM_CHROME_UI, null);
                    }
                });
        ChromeTabUtils.waitForTabPageLoaded(tab, testUrl);
        getInstrumentation().waitForIdleSync();

        // Kill the renderer and wait for the crash to be noted by the browser process.
        assertTrue(ChildProcessLauncher.crashProcessForTesting(
                tab.getContentViewCore().getCurrentRenderProcessId()));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Renderer crash wasn't noticed by the browser.") {
                    @Override
                    public boolean isSatisfied() {
                        return tab.getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                });

        // Reload the tab, respawning the renderer.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                tab.reload();
            }
        });

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

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Verify the visibility of the renderer.
                mBindingManager.assertIsInForeground(
                        tab.getContentViewCore().getCurrentRenderProcessId());
            }
        });
    }

    private int getRenderProcessId(final Tab tab) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return tab.getContentViewCore().getCurrentRenderProcessId();
            }
        });
    }

    /**
     * Ensures correctness of the visibilityDetermined() calls, that would be generally preceded by
     * setInForeground(), but it can't be guaranteed because they are triggered from different
     * threads.
     */
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testVisibilityDetermined() throws InterruptedException {
        // Create a tab in foreground and wait until it is loaded.
        final Tab fgTab = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Tab>() {
                    @Override
                    public Tab call() {
                        TabCreator tabCreator = getActivity().getCurrentTabCreator();
                        return tabCreator.createNewTab(
                                new LoadUrlParams(mTestServer.getURL(FILE_PATH)),
                                        TabLaunchType.FROM_CHROME_UI, null);
                    }});
        ChromeTabUtils.waitForTabPageLoaded(fgTab, mTestServer.getURL(FILE_PATH));
        int initialNavigationPid = getRenderProcessId(fgTab);
        // Ensure the following calls happened:
        //  - FG - setInForeground(true) - when the tab is created in the foreground
        //  - DETERMINED - visibilityDetermined() - after the initial navigation is committed
        assertEquals("FG;DETERMINED;", mBindingManager.getVisibilityCalls(initialNavigationPid));

        // Navigate to about:version which requires a different renderer.
        loadUrlInTab(ABOUT_VERSION_PATH, PageTransition.LINK, fgTab);
        int secondNavigationPid = getRenderProcessId(fgTab);
        MoreAsserts.assertNotEqual(secondNavigationPid, initialNavigationPid);
        // Ensure the following calls happened:
        //  - BG - setInForeground(false) - when the renderer is created for uncommited frame
        //  - FG - setInForeground(true) - when the frame is swapped in on commit
        //  - DETERMINED - visibilityDetermined() - after the navigation is committed
        // Or BG -> DETERMINED -> FG is also possible because setInForeground() and
        // visibilityDetermined() are triggered from different threads.
        mBindingManager.assertIsInForeground(secondNavigationPid);
        String visibilityCalls = mBindingManager.getVisibilityCalls(secondNavigationPid);
        assertTrue(visibilityCalls, "BG;FG;DETERMINED;".equals(visibilityCalls)
                || "BG;DETERMINED;FG;".equals(visibilityCalls));

        // Open a tab in the background and load it.
        final Tab bgTab = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Tab>() {
                    @Override
                    public Tab call() {
                        TabCreator tabCreator = getActivity().getCurrentTabCreator();
                        Tab tab = tabCreator.createNewTab(
                                new LoadUrlParams(mTestServer.getURL(FILE_PATH)),
                                        TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
                        // On Svelte devices the background tab would not be loaded automatically,
                        // so trigger the load manually.
                        tab.show(TabSelectionType.FROM_USER);
                        tab.hide();
                        return tab;
                    }});
        ChromeTabUtils.waitForTabPageLoaded(bgTab, mTestServer.getURL(FILE_PATH));
        int bgNavigationPid = getRenderProcessId(bgTab);
        // Ensure the following calls happened:
        //  - BG - setInForeground(false) - when tab is created in the background
        //  - DETERMINED - visibilityDetermined() - after the navigation is committed
        assertEquals("BG;DETERMINED;", mBindingManager.getVisibilityCalls(bgNavigationPid));
    }

    /**
     * When a user navigates to a prererendered URL, the tab swaps in the prerender's render
     * process and discards its old render process. Test that visibilityDetermined() is called for
     * the swapped in render process.
     */
    @LargeTest
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"ProcessManagement"})
    public void testVisibilityDeterminedNavigateToPrerenderedPage() throws InterruptedException {
        loadUrl(mTestServer.getURL(FILE_PATH));
        Tab tab = getActivity().getActivityTab();
        int pid1 = getRenderProcessId(tab);

        String prerenderUrl = mTestServer.getURL(FILE_PATH2);
        PrerenderTestHelper.prerenderUrl(prerenderUrl, tab);
        assertEquals(TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD, loadUrl(prerenderUrl));

        int pid2 = getRenderProcessId(tab);
        MoreAsserts.assertNotEqual(pid1, pid2);

        assertTrue(mBindingManager.getVisibilityCalls(pid1).contains("DETERMINED;"));
        assertTrue(mBindingManager.getVisibilityCalls(pid2).contains("DETERMINED;"));
    }

    /**
     * Verifies that BindingManager.releaseAllModerateBindings() is called once all the sandboxed
     * services are allocated.
     */
    @ChildProcessAllocatorSettings(sandboxedServiceCount = 4)
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testReleaseAllModerateBindings() throws InterruptedException {
        final TabCreator tabCreator = getActivity().getCurrentTabCreator();
        final Tab[] tabs = new Tab[3];
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                tabs[0] = tabCreator.createNewTab(
                        new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
                // Background tab.
                tabs[1] = tabCreator.createNewTab(
                        new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[0], "about:blank");
        ChromeTabUtils.waitForTabPageLoaded(tabs[1], "about:blank");
        // At this point 3 sanboxed services are allocated; the initial one + 2 new tabs.
        assertFalse(mBindingManager.isReleaseAllModerateBindingsCalled());

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Foreground tab.
                tabs[2] = tabCreator.createNewTab(
                        new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(tabs[2], "about:blank");
        // At this point all the sanboxed services are allocated.
        mBindingManager.assertIsReleaseAllModerateBindingsCalled();
    }

    // Test crashes on tablets. See crbug.com/594407
    @LargeTest
    @Feature({"ProcessManagement"})
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testRestoreSharedRenderer() throws Exception {
        loadUrl(mTestServer.getURL(SHARED_RENDERER_PAGE_PATH));

        final Tab[] tabs = new Tab[2];
        tabs[0] = getActivity().getActivityTab();
        singleClickView(tabs[0].getView());

        CriteriaHelper.pollInstrumentationThread(new Criteria("Child tab isn't opened.") {
            @Override
            public boolean isSatisfied() {
                return getActivity().getCurrentTabModel().getCount() == 2
                        && tabs[0] != getActivity().getActivityTab()
                        && getActivity()
                                   .getActivityTab()
                                   .getContentViewCore()
                                   .getCurrentRenderProcessId()
                        != 0;
            }
        });
        tabs[1] = getActivity().getActivityTab();
        assertEquals(tabs[0].getContentViewCore().getCurrentRenderProcessId(),
                tabs[1].getContentViewCore().getCurrentRenderProcessId());

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Verify the visibility of the renderer.
                mBindingManager.assertIsInForeground(
                        tabs[0].getContentViewCore().getCurrentRenderProcessId());
            }
        });

        assertTrue(ChildProcessLauncher.crashProcessForTesting(
                tabs[1].getContentViewCore().getCurrentRenderProcessId()));

        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Renderer crash wasn't noticed by the browser.") {
                    @Override
                    public boolean isSatisfied() {
                        return tabs[1].getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                });
        // Reload the tab, respawning the renderer.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                tabs[1].reload();
            }
        });

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

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // Verify the visibility of the renderer.
                mBindingManager.assertIsInForeground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId());
                tabs[1].hide();
                mBindingManager.assertIsInBackground(
                        tabs[1].getContentViewCore().getCurrentRenderProcessId());
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        // Hook in the test binding manager.
        mBindingManager = new MockBindingManager();
        ChildProcessLauncher.setBindingManagerForTesting(mBindingManager);

        super.setUp();

        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * @return the index of the given tab in the current tab model
     */
    private int indexOf(Tab tab) {
        return getActivity().getCurrentTabModel().indexOf(tab);
    }
}
