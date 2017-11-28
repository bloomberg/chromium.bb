// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-app',

  properties: {
    /**
     * Object containing current settings of Print Preview, for use by Polymer
     * controls.
     * @type {!Object}
     */
    settings: {
      type: Object,
      notify: true,
    },

    /** @type {print_preview.Destination} */
    destination: {
      type: Object,
      notify: true,
    },

    /** @type {print_preview.DocumentInfo} */
    documentInfo: {
      type: Object,
      notify: true,
    },

    /** @type {!print_preview_new.State} */
    state: {
      type: Object,
      notify: true,
    },
  },
});
