// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/** @enum {number} */
print_preview.State = {
  NOT_READY: 0,
  READY: 1,
  HIDDEN: 2,
  PRINTING: 3,
  SYSTEM_DIALOG: 4,
  ERROR: 5,
  FATAL_ERROR: 6,
  CLOSING: 7,
};

/** @enum {number} */
print_preview.Error = {
  NONE: 0,
  INVALID_TICKET: 1,
  INVALID_PRINTER: 2,
  UNSUPPORTED_PRINTER: 3,
  NO_DESTINATIONS: 4,
  NO_PLUGIN: 5,
  PREVIEW_FAILED: 6,
  PRINT_FAILED: 7,
  CLOUD_PRINT_ERROR: 8,
};

Polymer({
  is: 'print-preview-state',

  properties: {
    /** @type {!print_preview.State} */
    state: {
      type: Number,
      notify: true,
      value: print_preview.State.NOT_READY,
    },

    /** @type {!print_preview.Error} */
    error: {
      type: Number,
      notify: true,
      value: print_preview.Error.NONE,
    },
  },

  /** @param {print_preview.State} newState The state to transition to. */
  transitTo: function(newState) {
    switch (newState) {
      case (print_preview.State.NOT_READY):
        assert(
            this.state == print_preview.State.NOT_READY ||
            this.state == print_preview.State.READY ||
            this.state == print_preview.State.ERROR);
        break;
      case (print_preview.State.READY):
        assert(
            this.state == print_preview.State.ERROR ||
            this.state == print_preview.State.NOT_READY ||
            this.state == print_preview.State.PRINTING);
        break;
      case (print_preview.State.HIDDEN):
        assert(this.state == print_preview.State.READY);
        break;
      case (print_preview.State.PRINTING):
        assert(
            this.state == print_preview.State.READY ||
            this.state == print_preview.State.HIDDEN);
        break;
      case (print_preview.State.SYSTEM_DIALOG):
        assert(
            this.state != print_preview.State.HIDDEN &&
            this.state != print_preview.State.PRINTING &&
            this.state != print_preview.State.CLOSING);
        break;
      case (print_preview.State.ERROR):
        assert(
            this.state == print_preview.State.ERROR ||
            this.state == print_preview.State.NOT_READY ||
            this.state == print_preview.State.READY);
        break;
      case (print_preview.State.CLOSING):
        assert(this.state != print_preview.State.HIDDEN);
        break;
    }
    this.state = newState;
    if (newState !== print_preview.State.ERROR &&
        newState !== print_preview.State.FATAL_ERROR) {
      this.error = print_preview.Error.NONE;
    }
  },
});
