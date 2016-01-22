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
     * The currently selected item. This property is bound to the iron-selector
     * and never directly assigned.
     * @private {Element}
     */
    selectedItem_: {
      type: Element,
      notify: settings_test.changePictureOptions.notifyPropertyChangesForTest,
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
     * the camera, a file, or a deprecated default image. It defaults to an
     * empty string instead of undefined, because Polymer bindings don't behave
     * as expected with undefined properties.
     * @private {string}
     */
    oldImageUrl_: {
      type: String,
      value: '',
    },

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
       * Called from C++ to provide the URL of the selected image. Is only
       * called with default images.
       * @param {string} imageUrl
       */
      receiveSelectedImage: function(imageUrl) {
        var index = this.$.selector.items.findIndex(function(image) {
          return image.dataset.type == 'default' && image.src == imageUrl;
        });
        assert(index != -1, 'Default image not found: ' + imageUrl);
        this.$.selector.select(index);
      }.bind(this),

      /**
       * Called from C++ to provide the URL of the 'old' image. The 'old'
       * image is any selected non-profile and non-default image. It can be
       * from the camera, a file, or a deprecated default image. When this
       * method is called, it's implied that the old image is selected.
       * @param {string} imageUrl
       */
      receiveOldImage: function(imageUrl) {
        this.oldImageUrl_ = imageUrl;
        this.$.selector.select(this.$.selector.indexOf(this.$.oldImage));
      }.bind(this),

      /**
       * Called from C++ to provide the URL of the profile image.
       * @param {string} imageUrl
       * @param {boolean} selected
       */
      receiveProfileImage: function(imageUrl, selected) {
        this.profileImageUrl_ = imageUrl;
        if (selected)
          this.$.selector.select(this.$.selector.indexOf(this.$.profileImage));
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
   * Handler for when the an image is activated.
   * @param {!Event} event
   * @private
   */
  onImageActivate_: function(event) {
    var selectedImage = event.detail.item;
    switch (selectedImage.dataset.type) {
      case 'camera':
        // TODO(tommycli): Implement camera functionality.
        break;
      case 'profile':
        settings.ChangePicturePrivateApi.selectProfileImage();
        break;
      case 'old':
        settings.ChangePicturePrivateApi.selectOldImage();
        break;
      case 'default':
        settings.ChangePicturePrivateApi.selectDefaultImage(selectedImage.src);
        break;
      default:
        assertNotReached('Selected unknown image type');
    }
  },

  /**
   * True if there is no old image and the selection icon should be hidden.
   * @param {string} oldImageUrl
   * @return {boolean}
   * @private
   */
  isOldImageHidden_: function(oldImageUrl) {
    return oldImageUrl.length == 0;
  },

  /**
   * True if the preview image is hidden and the camera controls are shown.
   * @param {!Element} selectedItem
   * @return {boolean}
   * @private
   */
  isPreviewImageHidden_: function(selectedItem) {
    // The selected item can be undefined on initial load and between selection
    // changes. In those cases, show the preview image.
    return selectedItem != undefined && selectedItem.dataset.type == 'camera';
  },
});
