// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// SystemOptions class:

/**
 * Encapsulated handling of ChromeOS system options page.
 * @constructor
 */
function SystemOptions(model) {
  OptionsPage.call(this, 'system', templateData.systemPage, 'systemPage');
}

SystemOptions.getInstance = function() {
  if (SystemOptions.instance_)
    return SystemOptions.instance_;
  SystemOptions.instance_ = new SystemOptions(null);
  return SystemOptions.instance_;
}

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
      // TODO: Open ChromeOS language settings page.
    };
    $('sync-button').onclick = function(event) {
      OptionsPage.showPageByName('sync');
    }
    $('dummy-button').onclick = function(event) {
      OptionsPage.showOverlay('dummy');
    }
  },
};
