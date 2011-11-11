// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview AppListView implementation.
 */

cr.define('appList', function() {
  'use strict';

  /**
   * Creates an AppsView object.
   * @constructor
   * @extends {PageListView}
   */
  function AppsView() {
    this.initialize(getRequiredElement('page-list'),
                    getRequiredElement('dot-list'),
                    getRequiredElement('card-slider-frame'));
  }

  AppsView.prototype = {
    __proto__: ntp4.PageListView.prototype
  };

  // Return an object with all the exports
  return {
    AppsView: AppsView
  };
});
