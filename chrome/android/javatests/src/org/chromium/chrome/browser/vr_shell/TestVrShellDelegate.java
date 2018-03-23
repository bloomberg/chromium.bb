// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.os.Handler;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;

/**
 * Class for accessing VrShellDelegate internals for testing purposes.
 * This does two things:
 * - Prevents us from needing @VisibleForTesting annotations everywhere in production code
 * - Allows us to have test-specific behavior if necessary without changing production code
 */
public class TestVrShellDelegate extends VrShellDelegate {
    // Arbitrary but valid delay to make sure that we actually did cancel the DON flow instead of
    // running into crbug.com/762724
    private static final int DON_CANCEL_DELAY_MS = 200;
    private boolean mOnResumeCalled;
    private Runnable mOnVSyncPausedCallback;
    private static TestVrShellDelegate sInstance;
    private boolean mDisableVrBrowsing;

    protected TestVrShellDelegate(ChromeActivity activity) {
        super(activity, VrShellDelegate.getVrClassesWrapper());
    }

    public static void createTestVrShellDelegate(final ChromeActivity activity) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                sInstance = new TestVrShellDelegate(activity);
            }
        });
    }

    public static TestVrShellDelegate getInstance() {
        return sInstance;
    }

    public static boolean isDisplayingUrlForTesting() {
        return VrShellDelegate.isDisplayingUrl();
    }

    public static VrShell getVrShellForTesting() {
        return VrShellDelegate.getVrShell();
    }

    @Override
    public void shutdownVr(boolean disableVrMode, boolean stayingInChrome) {
        super.shutdownVr(disableVrMode, stayingInChrome);
    }

    public void overrideDaydreamApiForTesting(VrDaydreamApi api) {
        super.overrideDaydreamApi(api);
    }

    public void overrideVrCoreVersionCheckerForTesting(VrCoreVersionChecker versionChecker) {
        super.overrideVrCoreVersionChecker(versionChecker);
    }

    public void setFeedbackFrequencyForTesting(int frequency) {
        super.setFeedbackFrequency(frequency);
    }

    @Override
    public boolean isListeningForWebVrActivate() {
        return super.isListeningForWebVrActivate();
    }

    @Override
    public boolean isClearActivatePending() {
        return super.isClearActivatePending();
    }

    @Override
    public boolean isVrEntryComplete() {
        return super.isVrEntryComplete();
    }

    @Override
    public boolean isShowingDoff() {
        return super.isShowingDoff();
    }

    @Override
    public void acceptDoffPromptForTesting() {
        super.acceptDoffPromptForTesting();
    }

    @Override
    protected boolean isVrBrowsingEnabled() {
        if (mDisableVrBrowsing) return false;
        return super.isVrBrowsingEnabled();
    }

    public void setVrBrowsingDisabled(boolean disabled) {
        mDisableVrBrowsing = disabled;
    }

    public void setVrShellOnVSyncPausedCallback(Runnable callback) {
        mOnVSyncPausedCallback = callback;
    }

    /**
     * Wait a short period of time before running if we think the DON flow was cancelled.
     *
     * The same as the production onResume, except that in the case of mProbablyInDon still being
     * set, the decision that the DON flow was cancelled is delayed until later to see if the
     * broadcast is just late. This is caused by crbug.com/762724.
     * TODO(bsheedy): Remove this when the root cause is fixed.
     */
    @Override
    protected void onResume() {
        if (!getDonSucceeded() && getProbablyInDon()) {
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    TestVrShellDelegate.super.onResume();
                    mOnResumeCalled = true;
                }
            }, DON_CANCEL_DELAY_MS);
        } else {
            super.onResume();
            mOnResumeCalled = true;
        }
    }

    /**
     * Make sure that onResume is called before onPause.
     *
     * This is necessary since we don't want weird problems to show up caused by the delayed
     * onResume being called after onPause.
     */
    @Override
    protected void onPause() {
        Assert.assertTrue(mOnResumeCalled);
        mOnResumeCalled = false;
        super.onPause();
    }

    /**
     * If we need to know when the normal VSync gets paused, we have a small window between when
     * the VrShell is created and we actually enter VR to set the callback. So, do it immediately
     * after creation here.
     */
    @Override
    protected boolean createVrShell() {
        boolean result = super.createVrShell();
        if (result && mOnVSyncPausedCallback != null) {
            ((VrShellImpl) getVrShellForTesting())
                    .setOnVSyncPausedForTesting(mOnVSyncPausedCallback);
        }
        return result;
    }
}