// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.graphics.Rect;
import android.graphics.Region;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.preferences.NetworkPredictionOptions;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for InfoBars.
 *
 * TODO(newt): merge this with InfoBarTest after upstreaming.
 */
public class InfoBarTest2 extends ChromeActivityTestCaseBase<ChromeActivity> {
    static class MutableBoolean {
        public boolean mValue = false;
    }

    private InfoBarTestAnimationListener mListener;

    public InfoBarTest2() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Register for animation notifications
        InfoBarContainer container =
                getActivity().getActivityTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);
    }

    // Adds an infobar to the currrent tab. Blocks until the infobar has been added.
    protected void addInfoBarToCurrentTab(final InfoBar infoBar) throws InterruptedException {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Tab tab = getActivity().getActivityTab();
                assertTrue("Failed to find tab.", tab != null);
                tab.getInfoBarContainer().addInfoBar(infoBar);
            }
        });
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());
        getInstrumentation().waitForIdleSync();
    }

    // Removes an infobar from the currrent tab. Blocks until the infobar has been removed.
    protected void removeInfoBarFromCurrentTab(final InfoBar infoBar) throws InterruptedException {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Tab tab = getActivity().getActivityTab();
                assertTrue("Failed to find tab.", tab != null);
                tab.getInfoBarContainer().removeInfoBar(infoBar);
            }
        });
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        getInstrumentation().waitForIdleSync();
    }

    // Dismisses the passed infobar. Blocks until the bar has been removed.
    protected void dismissInfoBar(final InfoBar infoBar) throws InterruptedException {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                infoBar.dismissJavaOnlyInfoBar();
            }
        });
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        getInstrumentation().waitForIdleSync();
    }

    protected ArrayList<Integer> getInfoBarIdsForCurrentTab() {
        final ArrayList<Integer> infoBarIds = new ArrayList<Integer>();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Tab tab = getActivity().getActivityTab();
                assertTrue("Failed to find tab.", tab != null);
                for (InfoBar infoBar : tab.getInfoBarContainer().getInfoBars()) {
                    infoBarIds.add(infoBar.getId());
                }
            }
        });
        return infoBarIds;
    }

    /**
     * Verifies that infobars added from Java expire or not as expected.
     */
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarExpiration() throws InterruptedException {
        // First add an infobar that expires.
        final MutableBoolean dismissed = new MutableBoolean();
        MessageInfoBar expiringInfoBar = new MessageInfoBar("Hello!");
        expiringInfoBar.setDismissedListener(new InfoBarListeners.Dismiss() {
            @Override
            public void onInfoBarDismissed(InfoBar infoBar) {
                dismissed.mValue = true;
            }
        });
        expiringInfoBar.setExpireOnNavigation(true);
        addInfoBarToCurrentTab(expiringInfoBar);

        // Verify it's really there.
        ArrayList<Integer> infoBarIds = getInfoBarIdsForCurrentTab();
        assertEquals(1, infoBarIds.size());
        assertEquals(expiringInfoBar.getId(), infoBarIds.get(0).intValue());

        // Now navigate, it should expire.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/google.html"));
        assertTrue("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        assertTrue("InfoBar did not expire on navigation.", dismissed.mValue);
        infoBarIds = getInfoBarIdsForCurrentTab();
        assertTrue(infoBarIds.isEmpty());

        // Now test a non-expiring infobar.
        MessageInfoBar persistentInfoBar = new MessageInfoBar("Hello!");
        persistentInfoBar.setDismissedListener(new InfoBarListeners.Dismiss() {
            @Override
            public void onInfoBarDismissed(InfoBar infoBar) {
                dismissed.mValue = true;
            }
        });
        dismissed.mValue = false;
        persistentInfoBar.setExpireOnNavigation(false);
        addInfoBarToCurrentTab(persistentInfoBar);

        // Navigate, it should still be there.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/google.html"));
        assertFalse("InfoBar did expire on navigation.", dismissed.mValue);
        infoBarIds = getInfoBarIdsForCurrentTab();
        assertEquals(1, infoBarIds.size());
        assertEquals(persistentInfoBar.getId(), infoBarIds.get(0).intValue());

        // Close the infobar.
        dismissInfoBar(persistentInfoBar);
    }

    // Define function to pass parameter to Runnable to be used in testInfoBarExpirationNoPrerender.
    private Runnable setNetworkPredictionOptions(
            final NetworkPredictionOptions networkPredictionOptions) {
        return new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge.getInstance().setNetworkPredictionOptions(
                        networkPredictionOptions);
            }
        };
    };

    /**
     * Same as testInfoBarExpiration but with prerender turned-off.
     * The behavior when prerender is on/off is different as in the prerender case the infobars are
     * added when we swap tabs.
     * @throws InterruptedException
     */
    @MediumTest
    @Feature({"Browser"})
    public void testInfoBarExpirationNoPrerender() throws InterruptedException, ExecutionException {
        // Save prediction preference.
        NetworkPredictionOptions networkPredictionOption =
                ThreadUtils.runOnUiThreadBlocking(new Callable<NetworkPredictionOptions>() {
                    @Override
                    public NetworkPredictionOptions call() {
                        return PrefServiceBridge.getInstance().getNetworkPredictionOptions();
                    }
                });
        try {
            ThreadUtils.runOnUiThreadBlocking(setNetworkPredictionOptions(
                    NetworkPredictionOptions.NETWORK_PREDICTION_NEVER));
            testInfoBarExpiration();
        } finally {
            // Make sure we restore prediction preference.
            ThreadUtils.runOnUiThreadBlocking(setNetworkPredictionOptions(networkPredictionOption));
        }
    }

    /**
     * Tests that adding and then immediately removing an infobar works as expected (and does not
     * assert).
     */
    @MediumTest
    @Feature({"Browser"})
    public void testQuickAddOneAndRemove() throws InterruptedException {
        final InfoBar infoBar = new MessageInfoBar("Hello");
        addInfoBarToCurrentTab(infoBar);
        removeInfoBarFromCurrentTab(infoBar);
        assertTrue(getInfoBarIdsForCurrentTab().isEmpty());
    }

    /**
     * Tests that adding and then immediately dismissing an infobar works as expected (and does not
     * assert).
     */
    @MediumTest
    @Feature({"Browser"})
    public void testQuickAddOneAndDismiss() throws InterruptedException {
        final InfoBar infoBar = new MessageInfoBar("Hello");
        addInfoBarToCurrentTab(infoBar);
        dismissInfoBar(infoBar);
        assertTrue(getInfoBarIdsForCurrentTab().isEmpty());
    }

    /**
     * Tests that adding 2 infobars and then immediately removing the last one works as expected.
     * This scenario is special as the 2nd infobar does not even get added to the view hierarchy.
     */
    @MediumTest
    @Feature({"Browser"})
    public void testAddTwoAndRemoveOneQuick() throws InterruptedException {
        final InfoBar infoBar1 = new MessageInfoBar("One");
        final InfoBar infoBar2 = new MessageInfoBar("Two");
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Tab tab = getActivity().getActivityTab();
                assertTrue("Failed to find tab.", tab != null);
                tab.getInfoBarContainer().addInfoBar(infoBar1);
                tab.getInfoBarContainer().addInfoBar(infoBar2);
                tab.getInfoBarContainer().removeInfoBar(infoBar2);
            }
        });

        // We should get an infobar added event for the first infobar.
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());

        // But no infobar removed event as the 2nd infobar was removed before it got added.
        assertFalse("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        ArrayList<Integer> infoBarIds = getInfoBarIdsForCurrentTab();
        assertEquals(1, infoBarIds.size());
        assertEquals(infoBar1.getId(), infoBarIds.get(0).intValue());
    }

    /**
     * Tests that adding 2 infobars and then immediately dismissing the last one works as expected
     * This scenario is special as the 2nd infobar does not even get added to the view hierarchy.
     */
    @MediumTest
    @Feature({"Browser"})
    public void testAddTwoAndDismissOneQuick() throws InterruptedException {
        final InfoBar infoBar1 = new MessageInfoBar("One");
        final InfoBar infoBar2 = new MessageInfoBar("Two");
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Tab tab = getActivity().getActivityTab();
                assertTrue("Failed to find tab.", tab != null);
                tab.getInfoBarContainer().addInfoBar(infoBar1);
                tab.getInfoBarContainer().addInfoBar(infoBar2);
                infoBar2.dismissJavaOnlyInfoBar();
            }
        });

        // We should get an infobar added event for the first infobar.
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());

        // But no infobar removed event as the 2nd infobar was removed before it got added.
        assertFalse("InfoBar not removed.", mListener.removeInfoBarAnimationFinished());
        ArrayList<Integer> infoBarIds = getInfoBarIdsForCurrentTab();
        assertEquals(1, infoBarIds.size());
        assertEquals(infoBar1.getId(), infoBarIds.get(0).intValue());
    }

    /**
     * Tests that we don't assert when a tab is getting closed while an infobar is being shown and
     * had been removed.
     */
    @MediumTest
    @Feature({"Browser"})
    public void testCloseTabOnAdd() throws InterruptedException {
        loadUrl(TestHttpServerClient.getUrl(
                "chrome/test/data/android/google.html"));

        final InfoBar infoBar = new MessageInfoBar("Hello");
        addInfoBarToCurrentTab(infoBar);
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Tab tab = getActivity().getActivityTab();
                assertTrue("Failed to find tab.", tab != null);
                tab.getInfoBarContainer().removeInfoBar(infoBar);
                getActivity().getTabModelSelector().closeTab(tab);
            }
        });
    }

    /**
     * Tests that the x button in the infobar does close the infobar and that the event is not
     * propagated to the ContentView.
     * @MediumTest
     * @Feature({"Browser"})
     * Bug: http://crbug.com/172427
     */
    @FlakyTest
    public void testCloseButton() throws InterruptedException, TimeoutException {
        loadUrl(TestHttpServerClient.getUrl(
                "chrome/test/data/android/click_listener.html"));
        final InfoBar infoBar = new MessageInfoBar("Hello");
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Tab tab = getActivity().getActivityTab();
                assertTrue("Failed to find tab.", tab != null);
                tab.getInfoBarContainer().addInfoBar(infoBar);
            }
        });
        assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());

        // Now press the close button.
        assertTrue("Close button wasn't found", InfoBarUtil.clickCloseButton(infoBar));
        assertTrue("Infobar not removed.", mListener.removeInfoBarAnimationFinished());

        // The page should not have received the click.
        assertTrue("The page recieved the click.",
                !Boolean.parseBoolean(runJavaScriptCodeInCurrentTab("wasClicked")));
    }

    /**
     * Tests that adding and removing correctly manages the transparent region, which allows for
     * optimizations in SurfaceFlinger (less overlays).
     */
    @MediumTest
    @Feature({"Browser"})
    public void testAddAndDismissSurfaceFlingerOverlays() throws InterruptedException {
        final ViewGroup decorView = (ViewGroup)  getActivity().getWindow().getDecorView();
        final InfoBarContainer infoBarContainer =
                getActivity().getActivityTab().getInfoBarContainer();

        // Detect layouts. Note this doesn't actually need to be atomic (just final).
        final AtomicInteger layoutCount = new AtomicInteger();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                decorView.getViewTreeObserver().addOnGlobalLayoutListener(
                        new ViewTreeObserver.OnGlobalLayoutListener() {
                            @Override
                            public void onGlobalLayout() {
                                layoutCount.incrementAndGet();
                            }
                        });
            }
        });

        // First add an infobar.
        final InfoBar infoBar = new MessageInfoBar("Hello");
        addInfoBarToCurrentTab(infoBar);

        // A layout must occur to recalculate the transparent region.
        boolean layoutOccured = CriteriaHelper.pollForUIThreadCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return layoutCount.get() > 0;
                    }
                });
        assertTrue(layoutOccured);

        final Rect fullDisplayFrame = new Rect();
        final Rect fullDisplayFrameMinusContainer = new Rect();
        final Rect containerDisplayFrame = new Rect();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                decorView.getWindowVisibleDisplayFrame(fullDisplayFrame);
                decorView.getWindowVisibleDisplayFrame(fullDisplayFrameMinusContainer);
                fullDisplayFrameMinusContainer.bottom -= infoBarContainer.getHeight();
                int windowLocation[] = new int[2];
                infoBarContainer.getLocationInWindow(windowLocation);
                containerDisplayFrame.set(
                        windowLocation[0],
                        windowLocation[1],
                        windowLocation[0] + infoBarContainer.getWidth(),
                        windowLocation[1] + infoBarContainer.getHeight());

                // The InfoBarContainer subtracts itself from the transparent region.
                Region transparentRegion = new Region(fullDisplayFrame);
                infoBarContainer.gatherTransparentRegion(transparentRegion);
                assertEquals(transparentRegion.getBounds(), fullDisplayFrameMinusContainer);
            }
        });

        // Now remove the infobar.
        layoutCount.set(0);
        dismissInfoBar(infoBar);

        // A layout must occur to recalculate the transparent region.
        layoutOccured = CriteriaHelper.pollForUIThreadCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return layoutCount.get() > 0;
                    }
                });
        assertTrue(layoutOccured);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                // The InfoBarContainer should no longer be subtracted from the transparent region.
                // We really want assertTrue(transparentRegion.contains(containerDisplayFrame)),
                // but region doesn't have 'contains(Rect)', so we invert the test. So, the old
                // container rect can't touch the bounding rect of the non-transparent region).
                Region transparentRegion = new Region();
                decorView.gatherTransparentRegion(transparentRegion);
                Region opaqueRegion = new Region(fullDisplayFrame);
                opaqueRegion.op(transparentRegion, Region.Op.DIFFERENCE);
                assertFalse(opaqueRegion.getBounds().intersect(containerDisplayFrame));
            }
        });

        // Additional manual test that this is working:
        // - adb shell dumpsys SurfaceFlinger
        // - Observe that Clank's overlay size changes (or disappears if URLbar is also gone).
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
