// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-shared-usb-devices' is the settings shared usb devices subpage for
 * Crostini.
 */

Polymer({
  is: 'settings-crostini-shared-usb-devices',

  behaviors: [PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  },
});
