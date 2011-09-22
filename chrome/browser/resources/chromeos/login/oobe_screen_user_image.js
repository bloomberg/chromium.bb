// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe user image screen implementation.
 */

cr.define('oobe', function() {

  var UserImagesGrid = options.UserImagesGrid;
  var ButtonImages = UserImagesGrid.ButtonImages;

  /**
   * Creates a new oobe screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserImageScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  UserImageScreen.register = function() {
    var screen = $('user-image');
    UserImageScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  UserImageScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Currently selected user image index (take photo button is with zero
     * index).
     * @type {number}
     */
    selectedUserImage_: -1,

    /** @inheritDoc */
    decorate: function(element) {
      var imageGrid = $('user-image-grid');
      UserImagesGrid.decorate(imageGrid);

      imageGrid.addEventListener('change',
                                 this.handleSelection_.bind(this));
      imageGrid.addEventListener('activate',
                                 this.handleImageActivated_.bind(this));

      imageGrid.addItem(ButtonImages.TAKE_PHOTO,
                        undefined,
                        this.handleTakePhoto_.bind(this));

      // Photo image data (if present).
      this.photoImage_ = null;
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('userImageScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var okButton = this.ownerDocument.createElement('button');
      okButton.id = 'ok-button';
      okButton.textContent = localStrings.getString('okButtonText');
      okButton.addEventListener('click', function(e) {
        chrome.send('onUserImageAccepted');
      });
      return [ okButton ];
    },

    /**
     * Handles "Take photo" button activation.
     * @private
     */
    handleTakePhoto_: function() {
      chrome.send('takePhoto');
    },

    /**
     * Handles image activation (by pressing Enter).
     * @private
     */
    handleImageActivated_: function() {
      switch ($('user-image-grid').selectedItemUrl) {
      case ButtonImages.TAKE_PHOTO:
        this.handleTakePhoto_();
        break;
      }
    },

    /**
     * Handles selection change.
     * @private
     */
    handleSelection_: function() {
      var url = $('user-image-grid').selectedItemUrl;
      if (url)
        $('user-image-preview').src = url;
      // Do not send button selections.
      if (url == ButtonImages.TAKE_PHOTO) {
        $('ok-button').disabled = true;
      } else if (url) {
        $('ok-button').disabled = false;
        chrome.send('selectImage', [url]);
      }
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      Oobe.getInstance().headerHidden = true;
      $('user-image-grid').updateAndFocus();

      // Announce the name of the screen, if accessibility is on.
      $('user-image-aria-label').setAttribute(
          'aria-label', localStrings.getString('userImageScreenTitle'));
    },
  };

  /**
   * Adds or updates image with user photo and sets it as preview.
   * @param {string} photoUrl Image encoded as data URL.
   */
  UserImageScreen.setUserPhoto = function(photoUrl) {
    var imageGrid = $('user-image-grid');
    if (this.photoImage_)
      imageGrid.removeItem(this.photoImage_);
    this.photoImage_ = imageGrid.addItem(photoUrl, undefined, undefined, 1);
    imageGrid.selectedItem = this.photoImage_;
    imageGrid.focus();
  };

  /**
   * Appends received images to the list.
   * @param {Array.<string>} images An array of URLs to user images.
   */
  UserImageScreen.setUserImages = function(images) {
    var imageGrid = $('user-image-grid');
    for (var i = 0, url; url = images[i]; i++)
      imageGrid.addItem(url);
  };

  /**
   * Selects user image with the given URL.
   * @param {string} url URL of the image to select.
   */
  UserImageScreen.setSelectedImage = function(url) {
    var imageGrid = $('user-image-grid');
    imageGrid.selectedItemUrl = url;
    imageGrid.focus();
  };

  return {
    UserImageScreen: UserImageScreen
  };
});
