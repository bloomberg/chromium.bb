// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // ChangePictureOptions class:

  /**
   * Encapsulated handling of ChromeOS change picture options page.
   * @constructor
   */
  function ChangePictureOptions() {
    OptionsPage.call(
        this,
        'changePicture',
        localStrings.getString('changePicturePage'),
        'change-picture-page');
  }

  cr.addSingletonGetter(ChangePictureOptions);

  ChangePictureOptions.prototype = {
    // Inherit ChangePictureOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    /**
     * Initializes ChangePictureOptions page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      $('take-photo').addEventListener('click',
                                       this.handleTakePhoto__,
                                       false);
      $('choose-file').addEventListener('click',
                                        this.handleChooseFile__,
                                        false);
    },

    /**
     * Handler for when the user clicks on "Take photo" button.
     * @private
     * @param {Event} e Click Event.
     */
    handleTakePhoto__: function(e) {
      chrome.send('takePhoto');
    },

    /**
     * Handler for when the user clicks on "Choose a file" button.
     * @private
     * @param {Event} e Click Event.
     */
    handleChooseFile__: function(e) {
      chrome.send('chooseFile');
    },

  };

  // Export
  return {
    ChangePictureOptions: ChangePictureOptions
  };

});

