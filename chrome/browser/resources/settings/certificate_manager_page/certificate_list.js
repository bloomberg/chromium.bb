// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-certificate-list' is an element that displays a list
 * of certificates.
 */
Polymer({
  is: 'settings-certificate-list',

  properties: {
    /** @type {!Array<!Certificate>} */
    certificates: {
      type: Array,
      value: function() { return []; },
    },

    /** @type {!settings.CertificateType} */
    certificateType: String,
  },
});
