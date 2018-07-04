// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import org.chromium.chrome.browser.customtabs.dynamicmodule.IObjectWrapper;

interface IActivityHost {
  IObjectWrapper /* Context */ getActivityContext() = 0;

  void setBottomBarView(in IObjectWrapper /* View */ bottomBarView) = 1;

  void setOverlayView(in IObjectWrapper /* View */ overlayView) = 2;

  void setBottomBarHeight(int height) = 3;
}
