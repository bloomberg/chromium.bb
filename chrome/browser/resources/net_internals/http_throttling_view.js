// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information related to HTTP throttling.
 */
var HttpThrottlingView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function HttpThrottlingView() {
    assertFirstConstructorCall(HttpThrottlingView);

    // Call superclass's constructor.
    superClass.call(this, HttpThrottlingView.MAIN_BOX_ID);

    this.enableCheckbox_ = $(HttpThrottlingView.ENABLE_CHECKBOX_ID);
    this.enableCheckbox_.onclick = this.onEnableCheckboxClicked_.bind(this);

    g_browser.addHttpThrottlingObserver(this);
  }

  // ID for special HTML element in category_tabs.html
  HttpThrottlingView.TAB_HANDLE_ID = 'tab-handle-http-throttling';

  // IDs for special HTML elements in http_throttling_view.html
  HttpThrottlingView.MAIN_BOX_ID = 'http-throttling-view-tab-content';
  HttpThrottlingView.ENABLE_CHECKBOX_ID =
      'http-throttling-view-enable-checkbox';

  cr.addSingletonGetter(HttpThrottlingView);

  HttpThrottlingView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Gets informed that HTTP throttling has been enabled/disabled.
     * @param {boolean} enabled HTTP throttling has been enabled.
     */
    onHttpThrottlingEnabledPrefChanged: function(enabled) {
      this.enableCheckbox_.checked = enabled;
    },

    /**
     * Handler for the onclick event of the checkbox.
     */
    onEnableCheckboxClicked_: function() {
      g_browser.enableHttpThrottling(this.enableCheckbox_.checked);
    }
  };

  return HttpThrottlingView;
})();
