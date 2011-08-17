// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information related to HTTP throttling.
 */
var HttpThrottlingView = (function() {
  'use strict';

  // IDs for special HTML elements in http_throttling_view.html
  var MAIN_BOX_ID = 'http-throttling-view-tab-content';
  var ENABLE_CHECKBOX_ID = 'http-throttling-view-enable-checkbox';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function HttpThrottlingView() {
    assertFirstConstructorCall(HttpThrottlingView);

    // Call superclass's constructor.
    superClass.call(this, MAIN_BOX_ID);

    this.enableCheckbox_ = $(ENABLE_CHECKBOX_ID);
    this.enableCheckbox_.onclick = this.onEnableCheckboxClicked_.bind(this);

    g_browser.addHttpThrottlingObserver(this);
  }

  // ID for special HTML element in category_tabs.html
  HttpThrottlingView.TAB_HANDLE_ID = 'tab-handle-http-throttling';

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
