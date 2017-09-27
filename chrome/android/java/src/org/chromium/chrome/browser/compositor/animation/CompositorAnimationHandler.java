// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.animation;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.support.annotation.NonNull;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;

import java.util.ArrayList;

/**
 * The handler responsible for managing and pushing updates to all of the active
 * CompositorAnimators.
 */
public class CompositorAnimationHandler {
    /** A list of all the handler's animators. */
    private final ArrayList<CompositorAnimator> mAnimators = new ArrayList<>();

    /** This handler's update host. */
    private final LayoutUpdateHost mUpdateHost;

    /**
     * A cached copy of the list of {@link CompositorAnimator}s to prevent allocating a new list
     * every update.
     */
    private final ArrayList<CompositorAnimator> mCachedList = new ArrayList<>();

    /**
     * Whether or not an update has already been requested for the next frame due to an animation
     * starting.
     */
    private boolean mWasUpdateRequestedForAnimationStart;

    /**
     * Default constructor.
     * @param host A {@link LayoutUpdateHost} responsible for requesting frames when an animation
     *             updates.
     */
    public CompositorAnimationHandler(@NonNull LayoutUpdateHost host) {
        assert host != null;
        mUpdateHost = host;
    }

    /**
     * Add an animator to the list of known animators to start receiving updates.
     * @param animator The animator to start.
     */
    public final void registerAndStartAnimator(final CompositorAnimator animator) {
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator a) {
                mAnimators.remove(animator);
                animator.removeListener(this);
            }
        });
        mAnimators.add(animator);

        if (!mWasUpdateRequestedForAnimationStart) {
            mUpdateHost.requestUpdate();
            mWasUpdateRequestedForAnimationStart = true;
        }
    }

    /**
     * Push an update to all the currently running animators.
     * @param deltaTimeMs The time since the previous update in ms.
     */
    public final void pushUpdate(long deltaTimeMs) {
        mWasUpdateRequestedForAnimationStart = false;
        if (mAnimators.isEmpty()) return;

        // Do updates to the animators. Use a cloned list so the original list can be modified in
        // the update loop.
        mCachedList.addAll(mAnimators);
        for (int i = 0; i < mCachedList.size(); i++) {
            CompositorAnimator currentAnimator = mCachedList.get(i);
            currentAnimator.doAnimationFrame(deltaTimeMs);
            // Once the animation ends, it no longer needs to receive updates; remove it from the
            // handler's list of animations. Restarting the animation will re-add the animation to
            // this handler.
            if (currentAnimator.hasEnded()) mAnimators.remove(currentAnimator);
        }
        mCachedList.clear();

        mUpdateHost.requestUpdate();
    }

    /**
     * Clean up this handler.
     */
    public final void destroy() {
        mAnimators.clear();
    }

    /**
     * @return The number of animations that are active inside this handler.
     */
    @VisibleForTesting
    public int getActiveAnimationCount() {
        return mAnimators.size();
    }
}
