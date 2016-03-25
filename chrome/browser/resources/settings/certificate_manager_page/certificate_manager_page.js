// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-certificate-manager-page' is the settings page
 * containing SSL certificate settings.
 */
Polymer({
  is: 'settings-certificate-manager-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {number} */
    selected: {
      type: Number,
      value: 0,
    },

    /** @type {!Array<!Certificate>} */
    personalCerts: {
      type: Array,
      value: function() { return []; },
    },

    /** @type {!Array<!Certificate>} */
    serverCerts: {
      type: Array,
      value: function() { return []; },
    },

    /** @type {!Array<!Certificate>} */
    caCerts: {
      type: Array,
      value: function() { return []; },
    },

    /** @type {!Array<!Certificate>} */
    otherCerts: {
      type: Array,
      value: function() { return []; },
    },

    /** @private */
    certificateTypeEnum_: {
      type: Object,
      value: settings.CertificateType,
      readonly: true,
    },
  },

  /** @override */
  attached: function() {
    this.addWebUIListener('certificates-changed', this.set.bind(this));
    settings.CertificatesBrowserProxyImpl.getInstance().refreshCertificates();
  },

  /**
   * @param {number} selectedIndex
   * @param {number} tabIndex
   * @return {boolean} Whether to show tab at |tabIndex|.
   * @private
   */
  isTabSelected_: function(selectedIndex, tabIndex) {
    return selectedIndex == tabIndex;
  },
});
