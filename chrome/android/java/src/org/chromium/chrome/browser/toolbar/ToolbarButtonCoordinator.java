// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.support.annotation.IntDef;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The controller for a generic bottom toolbar button. This class handles all interactions that the
 * button has with the outside world.
 */
public class ToolbarButtonCoordinator {
    /** The mediator that handles events from outside the button. */
    private final ToolbarButtonMediator mMediator;

    @IntDef({ButtonVisibility.BROWSING_MODE, ButtonVisibility.TAB_SWITCHER_MODE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonVisibility {
        /* The button should be visible in browsing mode and not in tab switcher mode */
        int BROWSING_MODE = 0;
        /* The button should be visible in tab switcher mode and not in browsing mode */
        int TAB_SWITCHER_MODE = 1;
    }

    /**
     * Build the controller that manages the button.
     * @param view The button's view.
     */
    public ToolbarButtonCoordinator(View view) {
        PropertyModel model = new PropertyModel(ToolbarButtonProperties.ALL_KEYS);
        mMediator = new ToolbarButtonMediator(model);

        PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> processor =
                new PropertyModelChangeProcessor<>(model, view, new ToolbarButtonViewBinder());
        model.addObserver(processor);
    }

    /**
     * @param onClickListener An {@link OnClickListener} that is triggered when the button is
     *                        clicked.
     */
    public void setButtonListeners(
            OnClickListener onClickListener, OnLongClickListener onLongClickListener) {
        mMediator.setButtonListeners(onClickListener, onLongClickListener);
    }

    /**
     * @param overviewModeBehavior The overview mode manager.
     * @param buttonVisibility Whether the button should be shown for browsing mode vs tab switcher
     *                         mode.
     */
    public void setOverviewModeBehavior(
            OverviewModeBehavior overviewModeBehavior, @ButtonVisibility int buttonVisibility) {
        mMediator.setOverviewModeBehavior(overviewModeBehavior, buttonVisibility);
    }
}
