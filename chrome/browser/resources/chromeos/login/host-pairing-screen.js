// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('host-pairing-screen', (function() {
  'use strict';

  /** @const */ var CALLBACK_CONTEXT_READY = 'contextReady';

  return {
    onBeforeShow: function() {
      Oobe.getInstance().headerHidden = true;
    },

    /** @override */
    initialize: function() {
      this.send(CALLBACK_CONTEXT_READY);
    }
  };
})());
