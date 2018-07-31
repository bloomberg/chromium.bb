// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.empty;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * A helper {@link ViewBinder} responsible for gluing {@link EmptyProperties} to
 * {@link EmptyView}.
 */
class EmptyViewBinder implements ViewBinder<PropertyModel, EmptyView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, EmptyView view, PropertyKey propertyKey) {
        if (propertyKey == EmptyProperties.STATE) {
            view.setState(model.getValue(EmptyProperties.STATE));
        } else if (propertyKey == EmptyProperties.EMPTY_TEXT_RES_ID) {
            view.setEmptyText(model.getValue(EmptyProperties.EMPTY_TEXT_RES_ID));
        } else if (propertyKey == EmptyProperties.EMPTY_ICON_RES_ID) {
            view.setEmptyIcon(model.getValue(EmptyProperties.EMPTY_ICON_RES_ID));
        }
    }
}