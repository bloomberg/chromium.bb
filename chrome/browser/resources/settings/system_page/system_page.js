// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Settings that affect how Chrome interacts with the underlying
 * operating system (i.e. network, background processes, hardware).
 */

Polymer({
  is: 'settings-system-page',

  properties: {
    /** @type {settings.SystemPageDelegate} */
    delegate: {
      type: Object,
    },

    prefs: {
      type: Object,
      notify: true,
    },
  },

  /** @private */
  onChangeProxySettingsTap_: function() {
    this.delegate.changeProxySettings();
  },
});
