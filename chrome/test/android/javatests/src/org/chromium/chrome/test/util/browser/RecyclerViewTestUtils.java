// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.TimeoutException;

/**
 * Utilities for {@link RecyclerView}, to handle waiting for animation changes and other potential
 * flakiness sources.
 */
public final class RecyclerViewTestUtils {
    private RecyclerViewTestUtils() {}

    public static RecyclerView.ViewHolder waitForView(
            final RecyclerView recyclerView, final int position) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(position);

                if (viewHolder == null) {
                    updateFailureReason("Cannot find view holder for position " + position + ".");
                    return false;
                }

                if (viewHolder.itemView.getParent() == null) {
                    updateFailureReason("The view is not attached for position " + position + ".");
                    return false;
                }

                return true;
            }
        });

        waitForStableRecyclerView(recyclerView);

        return recyclerView.findViewHolderForAdapterPosition(position);
    }

    public static void waitForViewToDetach(final RecyclerView recyclerView, final View view)
            throws InterruptedException, TimeoutException {
        final CallbackHelper callback = new CallbackHelper();

        recyclerView.addOnChildAttachStateChangeListener(
                new RecyclerView.OnChildAttachStateChangeListener() {
                    @Override
                    public void onChildViewAttachedToWindow(View view) {}

                    @Override
                    public void onChildViewDetachedFromWindow(View detachedView) {
                        if (detachedView == view) {
                            recyclerView.removeOnChildAttachStateChangeListener(this);
                            callback.notifyCalled();
                        }
                    }
                });
        callback.waitForCallback("The view did not detach.", 0);

        waitForStableRecyclerView(recyclerView);
    }

    public static void waitForStableRecyclerView(final RecyclerView recyclerView) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (recyclerView.isComputingLayout()) {
                    updateFailureReason("The recycler view is computing layout.");
                    return false;
                }

                if (recyclerView.isLayoutFrozen()) {
                    updateFailureReason("The recycler view layout is frozen.");
                    return false;
                }

                if (recyclerView.isAnimating()) {
                    updateFailureReason("The recycler view is animating.");
                    return false;
                }

                return true;
            }
        });
    }
}
