// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  function SessionRestoreOverlay() {
    OptionsPage.call(this, 'sessionRestoreOverlay',
                     templateData.sessionRestoreOverlayTitle,
                     'sessionRestoreOverlay');
  };

  cr.addSingletonGetter(SessionRestoreOverlay);

  SessionRestoreOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('sessionRestoreOverlayOk').onclick = function() {
        OptionsPage.closeOverlay();
      };
    },
  };

  // Export
  return {
    SessionRestoreOverlay: SessionRestoreOverlay
  };
});
