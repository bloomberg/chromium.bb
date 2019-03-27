// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class TouchlessNewTabPageProperties {
    public static final PropertyModel.WritableIntPropertyKey INITIAL_SCROLL_POSITION =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<Callback<Integer>> SCROLL_POSITION_CALLBACK =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
            INITIAL_SCROLL_POSITION, SCROLL_POSITION_CALLBACK};
}
