// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link WarmupManager} */
public class WarmupManagerTest extends NativeLibraryTestBase {
    private WarmupManager mWarmupManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mWarmupManager = WarmupManager.getInstance();
            }
        });
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mWarmupManager.destroySpareWebContents();
            }
        });
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @UiThreadTest
    public void testNoSpareRendererOnLowEndDevices() {
        mWarmupManager.createSpareWebContents();
        assertFalse(mWarmupManager.hasSpareWebContents());
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCreateAndTakeSpareRenderer() {
        final AtomicBoolean isRenderViewReady = new AtomicBoolean();
        final AtomicReference<WebContents> webContentsReference = new AtomicReference<>();

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mWarmupManager.createSpareWebContents();
                assertTrue(mWarmupManager.hasSpareWebContents());
                WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
                assertNotNull(webContents);
                assertFalse(mWarmupManager.hasSpareWebContents());
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
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @UiThreadTest
    public void testTakeSpareWebContents() throws Exception {
        mWarmupManager.createSpareWebContents();
        WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
        assertNotNull(webContents);
        assertFalse(mWarmupManager.hasSpareWebContents());
        webContents.destroy();
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @UiThreadTest
    public void testTakeSpareWebContentsChecksArguments() throws Exception {
        mWarmupManager.createSpareWebContents();
        assertNull(mWarmupManager.takeSpareWebContents(true, false));
        assertNull(mWarmupManager.takeSpareWebContents(false, true));
        assertNull(mWarmupManager.takeSpareWebContents(true, true));
        assertTrue(mWarmupManager.hasSpareWebContents());
    }

    /** Checks that the View inflation works. */
    @SmallTest
    @UiThreadTest
    public void testInflateLayout() throws Exception {
        int layoutId = R.layout.custom_tabs_control_container;
        int toolbarId = R.layout.custom_tabs_toolbar;
        Context context = getInstrumentation().getTargetContext();
        mWarmupManager.initializeViewHierarchy(context, layoutId, toolbarId);
        assertTrue(mWarmupManager.hasViewHierarchyWithToolbar(layoutId));
    }
}
