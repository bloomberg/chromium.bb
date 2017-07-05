// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link WarmupManager} */
@RunWith(BaseJUnit4ClassRunner.class)
public class WarmupManagerTest {
    @Rule
    public final RuleChain mChain =
            RuleChain.outerRule(new ChromeBrowserTestRule()).around(new UiThreadTestRule());

    private WarmupManager mWarmupManager;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mWarmupManager = WarmupManager.getInstance();
            }
        });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mWarmupManager.destroySpareWebContents();
            }
        });
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    public void testNoSpareRendererOnLowEndDevices() throws Throwable {
        mWarmupManager.createSpareWebContents();
        Assert.assertFalse(mWarmupManager.hasSpareWebContents());
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCreateAndTakeSpareRenderer() {
        final AtomicBoolean isRenderViewReady = new AtomicBoolean();
        final AtomicReference<WebContents> webContentsReference = new AtomicReference<>();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mWarmupManager.createSpareWebContents();
                Assert.assertTrue(mWarmupManager.hasSpareWebContents());
                WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
                Assert.assertNotNull(webContents);
                Assert.assertFalse(mWarmupManager.hasSpareWebContents());
                WebContentsObserver observer = new WebContentsObserver(webContents) {
                    @Override
                    public void renderViewReady() {
                        isRenderViewReady.set(true);
                    }
                };

                // This is not racy because {@link WebContentsObserver} methods are called on the UI
                // thread by posting a task. See {@link RenderViewHostImpl::PostRenderViewReady}.
                webContents.addObserver(observer);
                webContentsReference.set(webContents);
            }
        });
        CriteriaHelper.pollUiThread(new Criteria("Spare renderer is not initialized") {
            @Override
            public boolean isSatisfied() {
                return isRenderViewReady.get();
            }
        });
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                webContentsReference.get().destroy();
            }
        });
    }

    /** Tests that taking a spare WebContents makes it unavailable to subsequent callers. */
    @Test
    @SmallTest
    @UiThreadTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTakeSpareWebContents() throws Throwable {
        mWarmupManager.createSpareWebContents();
        WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
        Assert.assertNotNull(webContents);
        Assert.assertFalse(mWarmupManager.hasSpareWebContents());
        webContents.destroy();
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTakeSpareWebContentsChecksArguments() throws Throwable {
        mWarmupManager.createSpareWebContents();
        Assert.assertNull(mWarmupManager.takeSpareWebContents(true, false));
        Assert.assertNull(mWarmupManager.takeSpareWebContents(false, true));
        Assert.assertNull(mWarmupManager.takeSpareWebContents(true, true));
        Assert.assertTrue(mWarmupManager.hasSpareWebContents());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testClearsDeadWebContents() throws Throwable {
        mWarmupManager.createSpareWebContents();
        mWarmupManager.mSpareWebContents.simulateRendererKilledForTesting(false);
        Assert.assertNull(mWarmupManager.takeSpareWebContents(false, false));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRecordSpareWebContentsStatus() throws Throwable {
        String name = WarmupManager.WEBCONTENTS_STATUS_HISTOGRAM;
        MetricsUtils.HistogramDelta createdDelta =
                new MetricsUtils.HistogramDelta(name, WarmupManager.WEBCONTENTS_STATUS_CREATED);
        MetricsUtils.HistogramDelta usedDelta =
                new MetricsUtils.HistogramDelta(name, WarmupManager.WEBCONTENTS_STATUS_USED);
        MetricsUtils.HistogramDelta killedDelta =
                new MetricsUtils.HistogramDelta(name, WarmupManager.WEBCONTENTS_STATUS_KILLED);
        MetricsUtils.HistogramDelta destroyedDelta =
                new MetricsUtils.HistogramDelta(name, WarmupManager.WEBCONTENTS_STATUS_DESTROYED);

        // Created, used.
        mWarmupManager.createSpareWebContents();
        Assert.assertEquals(1, createdDelta.getDelta());
        Assert.assertNotNull(mWarmupManager.takeSpareWebContents(false, false));
        Assert.assertEquals(1, usedDelta.getDelta());

        // Created, killed.
        mWarmupManager.createSpareWebContents();
        Assert.assertEquals(2, createdDelta.getDelta());
        Assert.assertNotNull(mWarmupManager.mSpareWebContents);
        mWarmupManager.mSpareWebContents.simulateRendererKilledForTesting(false);
        Assert.assertEquals(1, killedDelta.getDelta());
        Assert.assertNull(mWarmupManager.takeSpareWebContents(false, false));

        // Created, destroyed.
        mWarmupManager.createSpareWebContents();
        Assert.assertEquals(3, createdDelta.getDelta());
        Assert.assertNotNull(mWarmupManager.mSpareWebContents);
        mWarmupManager.destroySpareWebContents();
        Assert.assertEquals(1, destroyedDelta.getDelta());
    }

    /** Checks that the View inflation works. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testInflateLayout() throws Throwable {
        int layoutId = R.layout.custom_tabs_control_container;
        int toolbarId = R.layout.custom_tabs_toolbar;
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mWarmupManager.initializeViewHierarchy(context, layoutId, toolbarId);
        Assert.assertTrue(mWarmupManager.hasViewHierarchyWithToolbar(layoutId));
    }
}
