// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information related to HTTP throttling.
 * @constructor
 */
function HttpThrottlingView(mainBoxId, enableCheckboxId) {
  DivView.call(this, mainBoxId);

  this.enableCheckbox_ = document.getElementById(enableCheckboxId);
  this.enableCheckbox_.onclick = this.onEnableCheckboxClicked_.bind(this);

  g_browser.addHttpThrottlingObserver(this);
}

inherits(HttpThrottlingView, DivView);

/**
 * Gets informed that HTTP throttling has been enabled/disabled.
 * @param {boolean} enabled HTTP throttling has been enabled.
 */
HttpThrottlingView.prototype.onHttpThrottlingEnabledPrefChanged = function(
    enabled) {
  this.enableCheckbox_.checked = enabled;
};

/**
 * Handler for the onclick event of the checkbox.
 */
HttpThrottlingView.prototype.onEnableCheckboxClicked_ = function() {
  g_browser.enableHttpThrottling(this.enableCheckbox_.checked);
};