// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

cr.define('app_management.actions', function() {
  /**
   * @param {Array<appManagement.mojom.App>} apps
   */
  function addApps(apps) {
    return {
      name: 'add-apps',
      apps: apps,
    };
  }

  return {
    addApps: addApps,
  };
});
