// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// SystemOptions class:

/**
 * Encapsulated handling of ChromeOS system options page.
 * @constructor
 */
function SystemOptions() {
  OptionsPage.call(this, 'system', templateData.systemPage, 'systemPage');
}

cr.addSingletonGetter(SystemOptions);

// Inherit SystemOptions from OptionsPage.
SystemOptions.prototype = {
  __proto__: OptionsPage.prototype,

  /**
   * Initializes SystemOptions page.
   * Calls base class implementation to starts preference initialization.
   */
  initializePage: function() {
    OptionsPage.prototype.initializePage.call(this);
    var timezone = $('timezone-select');
    if (timezone) {
      timezone.initializeValues(templateData.timezoneList);
    }

    $('language-button').onclick = function(event) {
      OptionsPage.showPageByName('language');
    };
  }
};
