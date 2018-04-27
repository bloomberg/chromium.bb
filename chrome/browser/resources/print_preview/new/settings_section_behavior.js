// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview_new', function() {
  /**
   * Helper functions for settings sections that need to be collapsed and
   * expanded.
   * @polymerBehavior
   */
  const SettingsSectionBehavior = {
    properties: {
      available: {
        type: Boolean,
        reflectToAttribute: true,
      },

      collapsible: {
        type: Boolean,
        reflectToAttribute: true,
      },
    },

    show: function() {
      this.hidden = false;
    },

    hide: function() {
      this.hidden = true;
    },
  };

  return {SettingsSectionBehavior: SettingsSectionBehavior};
});
