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
 * An image element.
 * @typedef {{
 *   dataset: {
 *     type: string,
 *   },
 *   src: string,
 * }}
 */
settings.ChangePictureImageElement;

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
     * @private {settings.ChangePictureImageElement}
     */
    selectedItem_: {
      type: settings.ChangePictureImageElement,
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

    /**
     * The fallback image to be selected when the user discards the 'old' image.
     * This may be null if the user started with the 'old' image.
     * @private {settings.ChangePictureImageElement}
     */
    fallbackImage_: {
      type: settings.ChangePictureImageElement,
      value: null,
    },

    /**
     * Type of the last selected icon. This is used to jump back to the camera
     * after the user discards a newly taken photo.
     * @private {string}
     */
    lastSelectedImageType_: {
      type: String,
      value: '',
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

        this.fallbackImage_ = this.$.selector.items[index];

        // If user is currently taking a photo, do not steal the focus.
        if (!this.selectedItem_ || this.selectedItem_.dataset.type != 'camera')
          this.$.selector.select(index);
      }.bind(this),

      /**
       * Called from C++ to provide the URL of the 'old' image. The 'old'
       * image is any selected non-profile and non-default image. It can be
       * from the camera, a file, or a deprecated default image. When this
       * method is called, the old image becomes the selected image.
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

        if (!selected)
          return;

        this.fallbackImage_ = this.$.profileImage;

        // If user is currently taking a photo, do not steal the focus.
        if (!this.selectedItem_ || this.selectedItem_.dataset.type != 'camera')
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
   * Selects an image element.
   * @param {!settings.ChangePictureImageElement} image
   * @private
   */
  selectImage_: function(image) {
    switch (image.dataset.type) {
      case 'camera':
        // Nothing needs to be done.
        break;
      case 'file':
        settings.ChangePicturePrivateApi.chooseFile();
        break;
      case 'profile':
        settings.ChangePicturePrivateApi.selectProfileImage();
        break;
      case 'old':
        settings.ChangePicturePrivateApi.selectOldImage();
        break;
      case 'default':
        settings.ChangePicturePrivateApi.selectDefaultImage(image.src);
        break;
      default:
        assertNotReached('Selected unknown image type');
    }
  },

  /**
   * Handler for when the an image is activated.
   * @param {!Event} event
   * @private
   */
  onImageActivate_: function(event) {
    var image = event.detail.item;
    this.lastSelectedImageType_ = image.dataset.type;
    this.selectImage_(image);
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
   * Discard currently selected old image. Selects the first default icon.
   * Returns to the camera stream if the user had just taken a picture.
   * @private
   */
  onTapDiscardOldImage_: function() {
    this.oldImageUrl_ = '';

    if (this.lastSelectedImageType_ == 'camera')
      this.$.selector.select(this.$.selector.indexOf(this.$.cameraImage));

    if (this.fallbackImage_ != null) {
      this.selectImage_(this.fallbackImage_);
      return;
    }

    // If the user has not chosen an image since opening the subpage and
    // discards the current photo, select the first default image.
    assert(this.defaultImages_.length > 0);
    settings.ChangePicturePrivateApi.selectDefaultImage(
        this.defaultImages_[0].url);
  },

  /**
   * True if there is no old image and the selection icon should be hidden.
   * @param {string} oldImageUrl
   * @return {boolean}
   * @private
   */
  isOldImageHidden_: function(oldImageUrl) { return oldImageUrl.length == 0; },

  /**
   * Return true if the selected icon in the image grid is the camera.
   * @param {!settings.ChangePictureImageElement} selectedItem
   * @return {boolean}
   * @private
   */
  isCameraActive_: function(cameraPresent, selectedItem) {
    return cameraPresent &&
           selectedItem != undefined &&
           selectedItem.dataset.type == 'camera';
  },

  /**
   * Return true if the discard controls should be hidden.
   * @param {!settings.ChangePictureImageElement} selectedItem
   * @return {boolean}
   * @private
   */
  isDiscardHidden_: function(selectedItem) {
    return selectedItem == undefined || selectedItem.dataset.type != 'old';
  },
});
