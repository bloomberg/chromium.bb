// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-printing-page',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** The current active route. */
    currentRoute: {
      type: Object,
      notify: true,
    },
  },

<if expr="chromeos">
  /** @private */
  onTapCupsPrinters_: function() {
    this.$.pages.setSubpageChain(['cups-printers']);
  },
</if>

  /** @private */
  onTapCloudPrinters_: function() {
    this.$.pages.setSubpageChain(['cloud-printers']);
  },
});
