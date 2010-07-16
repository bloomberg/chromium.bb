// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// LanguageHangulOptions class:

/**
 * Encapsulated handling of ChromeOS language Hangul options page.
 * @constructor
 */
function LanguageHangulOptions() {
  OptionsPage.call(this, 'languageHangul', templateData.languageHangulPage,
                   'languageHangulPage');
}

cr.addSingletonGetter(LanguageHangulOptions);

// Inherit LanguageHangulOptions from OptionsPage.
LanguageHangulOptions.prototype = {
  __proto__: OptionsPage.prototype,

  /**
   * Initializes LanguageHangulOptions page.
   * Calls base class implementation to starts preference initialization.
   */
  initializePage: function() {
    OptionsPage.prototype.initializePage.call(this);
    var keyboardLayout = $('keyboard-layout-select');
    keyboardLayout.initializeValues(templateData.keyboardLayoutList)
  }
};
