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

      $('take-photo-button').addEventListener('click',
                                              this.handleTakePhoto_,
                                              false);
      $('choose-file-button').addEventListener('click',
                                               this.handleChooseFile_,
                                               false);
      chrome.send('getAvailableImages');
    },

    /**
     * Handler for when the user clicks on "Take photo" button.
     * @private
     * @param {Event} e Click Event.
     */
    handleTakePhoto_: function(e) {
      chrome.send('takePhoto');
      OptionsPage.navigateToPage('personal');
    },

    /**
     * Handler for when the user clicks on "Choose a file" button.
     * @private
     * @param {Event} e Click Event.
     */
    handleChooseFile_: function(e) {
      chrome.send('chooseFile');
      OptionsPage.navigateToPage('personal');
    },

    /**
     * Handler for when the user clicks on any available user image.
     * @private
     * @param {Event} e Click Event.
     */
    handleImageClick_: function(e) {
      chrome.send('selectImage', [e.target.src]);
      OptionsPage.navigateToPage('personal');
    },

    /**
     * Appends new image to the end of the image list.
     * @param {string} src A url for the user image.
     * @private
     */
    addUserImage_: function(src) {
      var imageElement = document.createElement('img');
      imageElement.src = src;
      imageElement.addEventListener('click',
                                    this.handleImageClick_,
                                    false);
      var divElement = document.createElement('div');
      divElement.classList.add('list-element');
      divElement.appendChild(imageElement);
      $('images-list').appendChild(divElement);
    },

    /**
     * Inserts received images before "Choose file" button.
     * @param {List} images A list of urls to user images.
     * @private
     */
    addUserImages_: function(images) {
      for (var i = 0; i < images.length; i++) {
        var imageUrl = images[i];
        this.addUserImage_(imageUrl);
      }
    },
  };

  // Forward public APIs to private implementations.
  [
    'addUserImages',
  ].forEach(function(name) {
    ChangePictureOptions[name] = function(value) {
      ChangePictureOptions.getInstance()[name + '_'](value);
    };
  });

  // Export
  return {
    ChangePictureOptions: ChangePictureOptions
  };

});

