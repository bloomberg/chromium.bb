// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.iph;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class binds the key functions IPH UI model and view.
 */
public class KeyFunctionsIPHViewBinder {
    public static void bind(
            PropertyModel model, KeyFunctionsIPHView keyFunctionsIPHView, PropertyKey propertyKey) {
        if (KeyFunctionsIPHProperties.TOOLTIP_VIEW == propertyKey) {
            keyFunctionsIPHView.setTooltipView(model.get(KeyFunctionsIPHProperties.TOOLTIP_VIEW));
        } else if (KeyFunctionsIPHProperties.IS_VISIBLE == propertyKey) {
            if (model.get(KeyFunctionsIPHProperties.IS_VISIBLE)) {
                keyFunctionsIPHView.show();
            } else {
                keyFunctionsIPHView.hide();
            }
        } else if (KeyFunctionsIPHProperties.IS_CURSOR_VISIBLE == propertyKey) {
            keyFunctionsIPHView.setCursorVisibility(
                    model.get(KeyFunctionsIPHProperties.IS_CURSOR_VISIBLE));
        }
    }
}
