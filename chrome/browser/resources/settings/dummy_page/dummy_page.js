// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-dummy-page' is a placeholder page for testing settings UI.
 */
Polymer('cr-settings-dummy-page', {
  publish: {
    /**
     * ID of the page.
     *
     * @attribute PAGE_ID
     * @const string
     * default 'dummy'
     */
    PAGE_ID: 'dummy',

    /**
     * Title for the page header and navigation menu.
     *
     * @attribute pageTitle
     * @type string
     * @default 'Dummy page'
     */
    pageTitle: 'Dummy page',

    /**
     * Name of the 'core-icon' to show.
     *
     * @attribute icon
     * @type string
     * @default 'add-circle'
     */
    icon: 'add-circle',
  },

  /**
   * Event handler for the toggle button for the 'cr-collapse'.
   *
   * @private
   */
  toggleCollapse_: function() {
    this.$.collapse.toggle();
  },
});
