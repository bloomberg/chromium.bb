// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * SupervisedUserLearnMore class.
   * Encapsulated handling of the 'Learn more...' overlay page.
   * @constructor
   * @class
   */
  function SupervisedUserLearnMoreOverlay() {
    OptionsPage.call(this, 'supervisedUserLearnMore',
                     loadTimeData.getString('supervisedUserLearnMoreTitle'),
                     'supervised-user-learn-more');
  };

  cr.addSingletonGetter(SupervisedUserLearnMoreOverlay);

  SupervisedUserLearnMoreOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('supervised-user-learn-more-done').onclick = function(event) {
        OptionsPage.closeOverlay();
      };
    },
  };

  // Export
  return {
    SupervisedUserLearnMoreOverlay: SupervisedUserLearnMoreOverlay,
  };
});
