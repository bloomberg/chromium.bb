// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// LanguageOptions class:

/**
 * Encapsulated handling of ChromeOS language options page.
 * @constructor
 */
function LanguageOptions(model) {
  OptionsPage.call(this, 'language', localStrings.getString('languagePage'),
                   'languagePage');
}

cr.addSingletonGetter(LanguageOptions);

// Inherit LanguageOptions from OptionsPage.
LanguageOptions.prototype = {
  __proto__: OptionsPage.prototype,

  /**
   * Initializes LanguageOptions page.
   * Calls base class implementation to starts preference initialization.
   */
  initializePage: function() {
    OptionsPage.prototype.initializePage.call(this);

    options.language.LanguageList.decorate($('language-options-list'));

    this.addEventListener('visibleChange',
                          cr.bind(this.handleVisibleChange_, this));
  },

  languageListInitalized_: false,

  /**
   * Handler for OptionsPage's visible property change event.
   * @param {Event} e Property change event.
   */
  handleVisibleChange_ : function(e) {
    if (!this.languageListInitalized_ && this.visible) {
      this.languageListInitalized_ = true;
      $('language-options-list').redraw();
    }
  }
};
