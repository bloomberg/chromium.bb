// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * This class is responsible for pushing updates to the Android view of the button. These
 * updates are pulled from the {@link PropertyModel} when a notification of an update is
 * received.
 */
public class ToolbarButtonViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> {
    /**
     * Build a binder that handles interaction between the model and the views that make up the
     * button.
     */
    public ToolbarButtonViewBinder() {}

    @Override
    public final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ToolbarButtonProperties.ON_CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.getValue(ToolbarButtonProperties.ON_CLICK_LISTENER));
        } else if (ToolbarButtonProperties.ON_LONG_CLICK_LISTENER == propertyKey) {
            view.setOnLongClickListener(
                    model.getValue(ToolbarButtonProperties.ON_LONG_CLICK_LISTENER));
        } else if (ToolbarButtonProperties.IS_VISIBLE == propertyKey) {
            view.setVisibility(model.getValue(ToolbarButtonProperties.IS_VISIBLE) ? View.VISIBLE
                                                                                  : View.INVISIBLE);
        } else {
            assert false : "Unhandled property detected in ToolbarButtonViewBinder!";
        }
    }
}
