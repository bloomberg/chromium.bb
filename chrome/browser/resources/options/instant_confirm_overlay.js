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
      };
      $('instantConfirmOk').onclick = function() {
        OptionsPage.closeOverlay();
        var instantDialogShown = $('instantDialogShown');
        Preferences.setBooleanPref(instantDialogShown.pref, true,
                                   instantDialogShown.metric);
        var instantEnabledCheckbox = $('instantEnableCheckbox');
        Preferences.setBooleanPref(instantEnableCheckbox.pref, true,
                                   instantEnableCheckbox.metric);
      };
    },
  };

  // Export
  return {
    InstantConfirmOverlay: InstantConfirmOverlay
  };
});
