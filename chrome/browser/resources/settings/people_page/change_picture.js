// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_test', function() {
  var changePictureOptions = settings_test.changePictureOptions || {
    /**
     * True if property changes should fire events for testing purposes.
     * @type {boolean}
     */
    notifyPropertyChangesForTest: false,
  };
  return {changePictureOptions: changePictureOptions};
});

/**
 * @fileoverview
 * 'settings-change-picture' is the settings subpage containing controls to
 * edit a ChromeOS user's picture.
 *
 * @group Chrome Settings Elements
 * @element settings-change-picture
 */
Polymer({
  is: 'settings-change-picture',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The currently selected profile image URL. May be a data URL.
     * @private {string}
     */
    selectedImageUrl_: {
      type: String,
      notify: settings_test.changePictureOptions.notifyPropertyChangesForTest,
    },

    /**
     * True only if the user has selected the camera icon. Old photos captured
     * from the camera are represented as the 'old' image.
     * @private {boolean}
     */
    cameraActive_: {
      type: Boolean,
      value: false,
    },

    /**
     * This differs from its default value only if the user has just captured
     * a new photo from the camera.
     * @private {string}
     */
    cameraImageUrl_: {
      type: String,
      value: settings.ChangePicturePrivateApi.ButtonImages.TAKE_PHOTO,
    },

    /**
     * This differs from its default value only if the user has just captured
     * a new photo from the camera.
     * @private {string}
     */
    cameraImageTitle_: {
      type: String,
      value: function() { return loadTimeData.getString('takePhoto'); },
    },

    /**
     * The url of the 'old' image, which is the existing image sourced from
     * the camera, a file, or a deprecated default image.
     * @private {string}
     */
    oldImageUrl_: String,

    /**
     * The url of the profile image.
     * @private {string}
     */
    profileImageUrl_: {
      type: String,
      value: settings.ChangePicturePrivateApi.ButtonImages.PROFILE_PICTURE,
    },

    /**
     * The default user images. Populated by ChangePicturePrivateApi.
     * @private {!Array<!settings.DefaultImage>}
     */
    defaultImages_: {
      type: Array,
      value: function() { return []; },
    },
  },

  /** @override */
  attached: function() {
    // This is the interface called by the C++ handler.
    var nativeInterface = {
      /**
       * Called from C++ to provide the default set of images.
       * @param {!Array<!settings.DefaultImage>} images
       */
      receiveDefaultImages: function(images) {
        this.defaultImages_ = images;
      }.bind(this),

      /**
       * Called from C++ to provide the URL of the selected image.
       * @param {string} imageUrl
       */
      receiveSelectedImage: function(imageUrl) {
        this.cameraActive_ = false;
        this.selectedImageUrl_ = imageUrl;
      }.bind(this),

      /**
       * Called from C++ to provide the URL of the 'old' image. The 'old'
       * image is any selected non-profile and non-default image. It can be
       * from the camera, a file, or a deprecated default image. When this
       * method is called, it's implied that the old image is selected.
       * @param {string} imageUrl
       */
      receiveOldImage: function(imageUrl) {
        this.cameraActive_ = false;
        this.oldImageUrl_ = imageUrl;
        this.selectedImageUrl_ = imageUrl;
      }.bind(this),

      /**
       * Called from C++ to provide the URL of the profile image.
       * @param {string} imageUrl
       * @param {boolean} selected
       */
      receiveProfileImage: function(imageUrl, selected) {
        this.profileImageUrl_ = imageUrl;
        if (selected) {
          this.cameraActive_ = false;
          this.selectedImageUrl_ = imageUrl;
        }
      }.bind(this),

      /**
       * Called from the C++ to notify page about camera presence.
       * @param {boolean} cameraPresent
       */
      receiveCameraPresence: function(cameraPresent) {
        // TODO(tommycli): Implement camera functionality.
      }.bind(this),
    };

    cr.define('settings', function() {
      var ChangePicturePage = nativeInterface;
      return {
        ChangePicturePage: ChangePicturePage,
      };
    });

    settings.ChangePicturePrivateApi.initialize();
  },

  /**
   * Handler for when the user clicks the camera image.
   * @private
   */
  onCameraImageTap_: function() {
    this.cameraActive_ = true;
  },

  /**
   * Handler for when the user clicks a new profile image.
   * @private
   * @param {!Event} event
   */
  onDefaultImageTap_: function(event) {
    var element = Polymer.dom(event).rootTarget;

    var imageUrl = null;
    if (element.nodeName == 'IMG')
      imageUrl = element.src;
    else if (element.dataset && element.dataset.imageUrl)
      imageUrl = element.dataset.imageUrl;

    if (imageUrl != null) {
      settings.ChangePicturePrivateApi.selectDefaultImage(imageUrl);
      // Button toggle state is instead controlled by the selected image URL.
      event.preventDefault();
    }
  },

  /**
   * Handler for when the user clicks the 'old' image.
   * @private
   * @param {!Event} event
   */
  onOldImageTap_: function(event) {
    settings.ChangePicturePrivateApi.selectOldImage();
    // Button toggle state is instead controlled by the selected image URL.
    event.preventDefault();
  },

  /**
   * Handler for when the user clicks the 'profile' image.
   * @private
   * @param {!Event} event
   */
  onProfileImageTap_: function(event) {
    settings.ChangePicturePrivateApi.selectProfileImage();
    // Button toggle state is instead controlled by the selected image URL.
    event.preventDefault();
  },

  /**
   * Computed binding determining which profile image button is toggled on.
   * @private
   * @param {string} imageUrl
   * @param {string} selectedImageUrl
   * @return {boolean}
   */
  isActiveImage_: function(cameraActive, imageUrl, selectedImageUrl) {
    return !cameraActive && (imageUrl == selectedImageUrl);
  },
});
