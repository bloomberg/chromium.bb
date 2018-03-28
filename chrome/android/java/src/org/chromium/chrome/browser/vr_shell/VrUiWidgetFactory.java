// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.content.Context;
import android.widget.PopupWindow;

import org.chromium.ui.widget.popups.UiWidgetFactory;

/**
 * The factory that creates VR compatible UI widgets.
 */
public class VrUiWidgetFactory extends UiWidgetFactory {
    public VrUiWidgetFactory() {}

    @Override
    public PopupWindow createPopupWindow(Context context) {
        return null;
    }
}
