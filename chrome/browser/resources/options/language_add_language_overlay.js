// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// AddLanguageOverlay class:

cr.define('options.language', function() {

  const OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of ChromeOS add language overlay page.
   * @constructor
   */
  function AddLanguageOverlay() {
    OptionsPage.call(this, 'addLanguageOverlay',
                     localStrings.getString('add_button'),
                     'add-language-overlay-page');
  }

  cr.addSingletonGetter(AddLanguageOverlay);

  AddLanguageOverlay.prototype = {
    // Inherit AddLanguageOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initializes AddLanguageOverlay page.
     * Calls base class implementation to starts preference initialization.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Set up the cancel button.
      $('add-language-overlay-cancel-button').onclick = function(e) {
        OptionsPage.clearOverlays();
      };

      // Create the language list with which users can add a language.
      // Note that we have about 40 languages.
      var addLanguageList = $('add-language-overlay-language-list');
      var languageListData = templateData.languageList;
      for (var i = 0; i < languageListData.length; i++) {
        var language = languageListData[i];
        var button = document.createElement('button');
        button.className = 'link-button';
        button.textContent = language.displayName;
        // If the native name is different, add it.
        if (language.displayName != language.nativeDisplayName) {
          button.textContent += ' - ' + language.nativeDisplayName;
        }
        button.languageCode = language.code;
        var li = document.createElement('li');
        li.languageCode = language.code;
        li.appendChild(button);
        addLanguageList.appendChild(li);
      }
    },
  };

  return {
    AddLanguageOverlay: AddLanguageOverlay
  };
});
