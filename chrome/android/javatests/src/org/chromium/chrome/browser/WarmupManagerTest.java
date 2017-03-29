// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.content.browser.test.NativeLibraryTestRule;
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
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Rule
    public UiThreadTestRule mUiThreadTestRule = new UiThreadTestRule();

    private WarmupManager mWarmupManager;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();
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
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    public void testNoSpareRendererOnLowEndDevices() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mWarmupManager.createSpareWebContents();
                Assert.assertFalse(mWarmupManager.hasSpareWebContents());
            }
        });
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
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTakeSpareWebContents() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mWarmupManager.createSpareWebContents();
                WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
                Assert.assertNotNull(webContents);
                Assert.assertFalse(mWarmupManager.hasSpareWebContents());
                webContents.destroy();
            }
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testTakeSpareWebContentsChecksArguments() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mWarmupManager.createSpareWebContents();
                Assert.assertNull(mWarmupManager.takeSpareWebContents(true, false));
                Assert.assertNull(mWarmupManager.takeSpareWebContents(false, true));
                Assert.assertNull(mWarmupManager.takeSpareWebContents(true, true));
                Assert.assertTrue(mWarmupManager.hasSpareWebContents());
            }
        });
    }

    /** Checks that the View inflation works. */
    @Test
    @SmallTest
    public void testInflateLayout() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                int layoutId = R.layout.custom_tabs_control_container;
                int toolbarId = R.layout.custom_tabs_toolbar;
                Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
                mWarmupManager.initializeViewHierarchy(context, layoutId, toolbarId);
                Assert.assertTrue(mWarmupManager.hasViewHierarchyWithToolbar(layoutId));
            }
        });
    }
}
