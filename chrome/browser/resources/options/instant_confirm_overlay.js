// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  function InstantConfirmOverlay() {
    OptionsPage.call(this, 'instantConfirm',
                     templateData.instantConfirmTitle,
                     'instantConfirmOverlay');
  };

  cr.addSingletonGetter(InstantConfirmOverlay);

  InstantConfirmOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('instantConfirmCancel').onclick = function() {
        OptionsPage.closeOverlay();
        $('instantEnabledCheckbox').checked = false;
      };

      $('instantConfirmOk').onclick = function() {
        OptionsPage.closeOverlay();
        chrome.send('enableInstant');
      };
    },
  };

  // Export
  return {
    InstantConfirmOverlay: InstantConfirmOverlay
  };
});
