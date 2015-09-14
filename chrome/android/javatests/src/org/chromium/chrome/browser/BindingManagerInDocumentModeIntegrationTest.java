// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.os.Build;
import android.test.suitebuilder.annotation.LargeTest;
import android.util.SparseArray;
import android.util.SparseBooleanArray;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.document.DocumentModeTestBase;
import org.chromium.chrome.browser.document.DocumentTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.BindingManager;
import org.chromium.content.browser.ChildProcessConnection;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Integration tests in document mode for the BindingManager API. This test plants a mock
 * BindingManager implementation and verifies that the signals it relies on are correctly delivered.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
public class BindingManagerInDocumentModeIntegrationTest extends DocumentModeTestBase {
    private static class MockBindingManager implements BindingManager {
        // Maps pid to the last received visibility state of the renderer.
        private final SparseBooleanArray mProcessInForegroundMap = new SparseBooleanArray();
        // Maps pid to a string recording calls to setInForeground() and visibilityDetermined().
        private final SparseArray<String> mVisibilityCallsMap = new SparseArray<String>();
        private boolean mIsReleaseAllModerateBindingsCalled;

        void assertIsInForeground(final int pid) {
            try {
                assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mProcessInForegroundMap.get(pid);
                    }
                }));
            } catch (InterruptedException ie) {
                fail();
            }
        }

        void assertIsInBackground(final int pid) {
            try {
                assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !mProcessInForegroundMap.get(pid);
                    }
                }));
            } catch (InterruptedException ie) {
                fail();
            }
        }

        void assertSetInForegroundWasCalled(String message, final int pid) {
            try {
                assertTrue(message, CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mProcessInForegroundMap.indexOfKey(pid) >= 0;
                    }
                }));
            } catch (InterruptedException ie) {
                fail();
            }
        }

        void assertIsReleaseAllModerateBindingsCalled() {
            try {
                assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mIsReleaseAllModerateBindingsCalled;
                    }
                }));
            } catch (InterruptedException ie) {
                fail();
            }
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
        public void determinedVisibility(int pid) {
            synchronized (mVisibilityCallsMap) {
                mVisibilityCallsMap.put(pid, mVisibilityCallsMap.get(pid) + "DETERMINED;");
            }
        }

        @Override
        public void onSentToBackground() {}

        @Override
        public void onBroughtToForeground() {}

        @Override
        public boolean isOomProtected(int pid) {
            return false;
        }

        @Override
        public void clearConnection(int pid) {}

        @Override
        public void startModerateBindingManagement(
                Context context, int maxSize, float lowReduceRatio, float highReduceRatio) {}

        @Override
        public void releaseAllModerateBindings() {
            mIsReleaseAllModerateBindingsCalled = true;
        }
    }

    private MockBindingManager mBindingManager;

    // about:version will always be handled by a different renderer than a local file.
    private static final String ABOUT_VERSION_PATH = "chrome://version/";

    /**
     * Verifies that the .setProcessInForeground() signal is called correctly as the tabs are
     * created and switched.
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testTabSwitching() throws Exception {
        // Create two tabs and wait until they are loaded, so that their renderers are around.
        final Tab[] tabs = new Tab[2];
        final int[] tabIds = new int[2];
        final DocumentTabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
        tabIds[0] = launchViaViewIntent(false, URL_1, "Page 1");
        tabIds[1] = launchViaLaunchDocumentInstanceInBackground(false, URL_2, "Page 2");

        tabs[0] = selector.getTabById(tabIds[0]);
        tabs[1] = selector.getTabById(tabIds[1]);

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

        switchToTab((DocumentTab) tabs[1]);

        // Verify that the renderer visibility was flipped.
        mBindingManager.assertIsInBackground(
                tabs[0].getContentViewCore().getCurrentRenderProcessId());
        mBindingManager.assertIsInForeground(
                tabs[1].getContentViewCore().getCurrentRenderProcessId());
    }

    /**
     * Verifies that a renderer that crashes in foreground has the correct visibility when
     * recreated.
     */
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testCrashInForeground() throws Exception {
        // Create a tab in foreground and wait until it is loaded.
        final Tab tab = ChromeApplication.getDocumentTabModelSelector().getTabById(
                launchViaViewIntent(false, URL_1, "Page 1"));

        // Kill the renderer and wait for the crash to be noted by the browser process.
        assertTrue(ChildProcessLauncher.crashProcessForTesting(
                tab.getContentViewCore().getCurrentRenderProcessId()));

        assertTrue("Renderer crash wasn't noticed by the browser.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return tab.getContentViewCore().getCurrentRenderProcessId() == 0;
                    }
                }));

        // Reload the tab, respawning the renderer.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                tab.reload();
            }
        });

        // Wait until the process is spawned and its visibility is determined.
        assertTrue("Process for the crashed tab was not respawned.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return tab.getContentViewCore().getCurrentRenderProcessId() != 0;
                    }
                }));

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

    /**
     * Ensures correctness of the visibilityDetermined() calls, that should be always preceded by
     * setInForeground().
     */
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testVisibilityDetermined() throws Exception {
        // Create a tab in foreground and wait until it is loaded.
        final Tab fgTab = ChromeApplication.getDocumentTabModelSelector().getTabById(
                launchViaViewIntent(false, URL_1, "Page 1"));
        int initialNavigationPid = fgTab.getContentViewCore().getCurrentRenderProcessId();
        // Ensure the following calls happened:
        //  - FG - setInForeground(true) - when the tab is created in the foreground
        //  - DETERMINED - visibilityDetermined() - after the initial navigation is committed
        assertEquals("FG;DETERMINED;", mBindingManager.getVisibilityCalls(initialNavigationPid));

        // Navigate to about:version which requires a different renderer.
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                fgTab.loadUrl(new LoadUrlParams(ABOUT_VERSION_PATH));
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(fgTab, ABOUT_VERSION_PATH);
        int secondNavigationPid = fgTab.getContentViewCore().getCurrentRenderProcessId();
        assertTrue(secondNavigationPid != initialNavigationPid);
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
        final Tab bgTab = ChromeApplication.getDocumentTabModelSelector().getTabById(
                launchViaLaunchDocumentInstanceInBackground(false, URL_2, "Page 2"));
        int bgNavigationPid = bgTab.getContentViewCore().getCurrentRenderProcessId();
        // Ensure the following calls happened:
        //  - BG - setInForeground(false) - when tab is created in the background
        //  - DETERMINED - visibilityDetermined() - after the navigation is committed
        assertEquals("BG;DETERMINED;", mBindingManager.getVisibilityCalls(bgNavigationPid));
    }

    /**
     * Verifies that BindingManager.releaseAllModerateBindings() is called once all the sandboxed
     * services are allocated.
     */
    @CommandLineFlags.Add(ChildProcessLauncher.SWITCH_NUM_SANDBOXED_SERVICES_FOR_TESTING + "=4")
    @LargeTest
    @Feature({"ProcessManagement"})
    public void testReleaseAllModerateBindings() throws Exception {
        launchViaViewIntent(false, URL_1, "Page 1");
        launchViaViewIntent(false, URL_1, "Page 1");
        launchViaViewIntent(false, URL_1, "Page 1");
        // At this point 3 sanboxed services are allocated.
        assertFalse(mBindingManager.isReleaseAllModerateBindingsCalled());

        launchViaViewIntent(false, URL_1, "Page 1");
        // At this point all the sanboxed services are allocated.
        mBindingManager.assertIsReleaseAllModerateBindingsCalled();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // Hook in the test binding manager.
        mBindingManager = new MockBindingManager();
        ChildProcessLauncher.setBindingManagerForTesting(mBindingManager);
    }
}
