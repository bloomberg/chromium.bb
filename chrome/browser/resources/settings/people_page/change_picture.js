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
     * True if the user has a plugged-in webcam.
     * @private {boolean}
     */
    cameraPresent_: {
      type: Boolean,
      value: false,
    },

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
       * method is called for the first time, it is implied to be the selected
       * image (unless the user just took an image from the camera).
       * @param {string} imageUrl
       */
      receiveOldImage: function(imageUrl) {
        var oldImageAlreadyExists = this.oldImageUrl_.length > 0;
        this.oldImageUrl_ = imageUrl;

        var cameraSelected =
            this.selectedItem_ && this.selectedItem_.dataset.type == 'camera';
        if (!oldImageAlreadyExists && !cameraSelected)
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
        this.cameraPresent_ = cameraPresent;
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
        // Nothing needs to be done.
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
   * Handle photo captured event, which contains the data URL of the image.
   * @param {!{detail: !{photoDataUrl: string}}} event
   * containing a data URL.
   */
  onPhotoTaken_: function(event) {
    settings.ChangePicturePrivateApi.photoTaken(event.detail.photoDataUrl);

    // TODO(tommycli): Add announce of accessible message for photo capture.
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
   * Return true if the selected icon in the image grid is the camera.
   * @param {!Element} selectedItem
   * @return {boolean}
   * @private
   */
  isCameraActive_: function(cameraPresent, selectedItem) {
    return cameraPresent &&
           selectedItem != undefined &&
           selectedItem.dataset.type == 'camera';
  },
});
