// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for the App Management page.
 */

cr.define('app_management.util', function() {
  /** @return {!AppManagementPageState} */
  function createEmptyState() {
    return {apps: {}};
  }

  return {
    createEmptyState: createEmptyState,
  };
});
