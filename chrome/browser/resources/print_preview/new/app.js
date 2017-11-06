// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-app',

  properties: {
    /** @private {!print_preview_new.Model} */
    model_: {
      type: Object,
      notify: true,
      value: {
        previewLoading: false,
        previewFailed: false,
        cloudPrintError: '',
        privetExtensionError: '',
        invalidSettings: false,
        destinationId: 'Foo Printer',
        copies: 1,
        pageRange: [1],
        duplex: false,
        printTicketInvalid: false
      },
    },
  }
});
