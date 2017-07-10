// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-change-picture' is the settings subpage containing controls to
 * edit a ChromeOS user's picture.
 */
Polymer({
  is: 'settings-change-picture',

  behaviors: [
    settings.RouteObserverBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
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
     * and never directly assigned. This may be undefined momentarily as
     * the selection changes due to iron-selector implementation details.
     * @private {?CrPicture.ImageElement}
     */
    selectedItem_: Object,

    /**
     * The url of the Old image, which is the existing image sourced from
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
      value: 'chrome://theme/IDR_PROFILE_PICTURE_LOADING',
    },

    /**
     * The default user images.
     * @private {!Array<!settings.DefaultImage>}
     */
    defaultImages_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @private */
    selectionTypesEnum_: {
      type: Object,
      value: CrPicture.SelectionTypes,
      readOnly: true,
    },
  },

  listeners: {
    'discard-image': 'onDiscardImage_',
    'photo-flipped': 'onPhotoFlipped_',
    'photo-taken': 'onPhotoTaken_',
  },

  /** @private {?settings.ChangePictureBrowserProxy} */
  browserProxy_: null,

  /**
   * The fallback image to be selected when the user discards the Old image.
   * This may be null if the user started with the Old image.
   * @private {?CrPicture.ImageElement}
   */
  fallbackImage_: null,

  /**
   * Type of the last selected icon. This is used to jump back to the camera
   * after the user discards a newly taken photo.
   * @private {string}
   */
  lastSelectedImageType_: '',

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.ChangePictureBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'default-images-changed', this.receiveDefaultImages_.bind(this));
    this.addWebUIListener(
        'selected-image-changed', this.receiveSelectedImage_.bind(this));
    this.addWebUIListener(
        'old-image-changed', this.receiveOldImage_.bind(this));
    this.addWebUIListener(
        'profile-image-changed', this.receiveProfileImage_.bind(this));
    this.addWebUIListener(
        'camera-presence-changed', this.receiveCameraPresence_.bind(this));
  },


  /** @protected */
  currentRouteChanged: function(newRoute) {
    if (newRoute == settings.routes.CHANGE_PICTURE) {
      this.browserProxy_.initialize();

      // This in needed because we manually clear the selectedItem_ property
      // when navigating away. The selector element doesn't fire its upward
      // data binding unless its selected item has changed.
      this.selectedItem_ = this.$.selector.selectedItem;
      // Focus the container by default so that the arrow keys work (and since
      // we use the focus highlight to show which picture is selected).
      this.$.container.focus();
    } else {
      // Ensure we deactivate the camera when we navigate away.
      this.selectedItem_ = null;
    }
  },

  /**
   * Handler for the 'default-images-changed' event.
   * @param {!Array<!settings.DefaultImage>} images
   * @private
   */
  receiveDefaultImages_: function(images) {
    this.defaultImages_ = images;
  },

  /**
   * Handler for the 'selected-image-changed' event. Is only called with
   * default images.
   * @param {string} imageUrl
   * @private
   */
  receiveSelectedImage_: function(imageUrl) {
    var index = this.$.selector.items.findIndex(function(image) {
      return image.dataset.type == CrPicture.SelectionTypes.DEFAULT &&
          image.src == imageUrl;
    });
    assert(index != -1, 'Default image not found: ' + imageUrl);

    this.fallbackImage_ = this.$.selector.items[index];

    // If user is currently taking a photo, do not steal the focus.
    if (!this.selectedItem_ ||
        this.selectedItem_.dataset.type != CrPicture.SelectionTypes.CAMERA) {
      this.$.selector.select(index);
    }
  },

  /**
   * Handler for the 'old-image-changed' event. The Old image is any selected
   * non-profile and non-default image. It can be from the camera, a file, or a
   * deprecated default image. When this method is called, the Old image
   * becomes the selected image.
   * @param {string} imageUrl
   * @private
   */
  receiveOldImage_: function(imageUrl) {
    this.oldImageUrl_ = imageUrl;
    this.$.selector.select(this.$.selector.indexOf(this.$.oldImage));
  },

  /**
   * Handler for the 'profile-image-changed' event.
   * @param {string} imageUrl
   * @param {boolean} selected
   * @private
   */
  receiveProfileImage_: function(imageUrl, selected) {
    this.profileImageUrl_ = imageUrl;
    this.$.profileImage.title = this.i18n('profilePhoto');

    if (!selected)
      return;

    this.fallbackImage_ = this.$.profileImage;

    // If user is currently taking a photo, do not steal the focus.
    if (!this.selectedItem_ ||
        this.selectedItem_.dataset.type != CrPicture.SelectionTypes.CAMERA) {
      this.$.selector.select(this.$.selector.indexOf(this.$.profileImage));
    }
  },

  /**
   * Handler for the 'camera-presence-changed' event.
   * @param {boolean} cameraPresent
   * @private
   */
  receiveCameraPresence_: function(cameraPresent) {
    this.cameraPresent_ = cameraPresent;
  },

  /**
   * Selects an image element.
   * @param {!CrPicture.ImageElement} image
   * @private
   */
  selectImage_: function(image) {
    switch (image.dataset.type) {
      case CrPicture.SelectionTypes.CAMERA:
        // Nothing needs to be done.
        break;
      case CrPicture.SelectionTypes.FILE:
        this.browserProxy_.chooseFile();
        break;
      case CrPicture.SelectionTypes.PROFILE:
        this.browserProxy_.selectProfileImage();
        break;
      case CrPicture.SelectionTypes.OLD:
        this.browserProxy_.selectOldImage();
        break;
      case CrPicture.SelectionTypes.DEFAULT:
        this.browserProxy_.selectDefaultImage(image.src);
        break;
      default:
        assertNotReached('Selected unknown image type');
    }
  },

  /**
   * Handler for when accessibility-specific keys are pressed.
   * @param {!{detail: !{key: string, keyboardEvent: Object}}} e
   */
  onKeysPress_: function(e) {
    if (!this.selectedItem_)
      return;

    // In the old Options user images grid, the 'up' and 'down' keys had
    // different behavior depending on whether ChromeVox was on or off.
    // If ChromeVox was on, 'up' or 'down' would select the next or previous
    // image on the left or right. If ChromeVox was off, it would select the
    // image spatially above or below using calculated columns.
    //
    // The code below implements the simple behavior of selecting the image
    // to the left or right (as if ChromeVox was always on).
    //
    // TODO(tommycli): Investigate if it's necessary to calculate columns
    // and select the image on the top or bottom for non-ChromeVox users.
    var /** IronSelectorElement */ selector = this.$.selector;
    switch (e.detail.key) {
      case 'up':
      case 'left':
        // This loop always terminates because the file and profile icons are
        // never hidden.
        do {
          selector.selectPrevious();
        } while (this.selectedItem_.hidden);

        if (this.selectedItem_.dataset.type != CrPicture.SelectionTypes.FILE)
          this.selectImage_(this.selectedItem_);

        this.lastSelectedImageType_ = this.selectedItem_.dataset.type;
        e.detail.keyboardEvent.preventDefault();
        break;

      case 'down':
      case 'right':
        // This loop always terminates because the file and profile icons are
        // never hidden.
        do {
          selector.selectNext();
        } while (this.selectedItem_.hidden);

        if (this.selectedItem_.dataset.type != CrPicture.SelectionTypes.FILE)
          this.selectImage_(this.selectedItem_);

        this.lastSelectedImageType_ = this.selectedItem_.dataset.type;
        e.detail.keyboardEvent.preventDefault();
        break;

      case 'enter':
      case 'space':
        if (this.selectedItem_.dataset.type ==
            CrPicture.SelectionTypes.CAMERA) {
          /** CrPicturePreviewElement */ (this.$.picturePreview).takePhoto();
        } else if (
            this.selectedItem_.dataset.type == CrPicture.SelectionTypes.FILE) {
          this.browserProxy_.chooseFile();
        } else if (
            this.selectedItem_.dataset.type == CrPicture.SelectionTypes.OLD) {
          this.onDiscardImage_();
        }
        break;
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
   * @param {!{detail: !{photoDataUrl: string}}} event
   * @private
   */
  onPhotoTaken_: function(event) {
    this.browserProxy_.photoTaken(event.detail.photoDataUrl);
    this.$.container.focus();
    announceAccessibleMessage(
        loadTimeData.getString('photoCaptureAccessibleText'));
  },

  /**
   * @param {!{detail: boolean}} event
   * @private
   */
  onPhotoFlipped_: function(event) {
    var flipped = event.detail;
    var flipMessageId = flipped ? 'photoFlippedAccessibleText' :
                                  'photoFlippedBackAccessibleText';
    announceAccessibleMessage(loadTimeData.getString(flipMessageId));
  },

  /**
   * Discard currently selected image. Selects the first default icon.
   * Returns to the camera stream if the user had just taken a picture.
   * @private
   */
  onDiscardImage_: function() {
    this.oldImageUrl_ = '';

    if (this.lastSelectedImageType_ == CrPicture.SelectionTypes.CAMERA)
      this.$.selector.select(this.$.selector.indexOf(this.$.cameraImage));

    if (this.fallbackImage_ != null) {
      this.selectImage_(this.fallbackImage_);
      return;
    }

    // If the user has not chosen an image since opening the subpage and
    // discards the current photo, select the first default image.
    assert(this.defaultImages_.length > 0);
    this.browserProxy_.selectDefaultImage(this.defaultImages_[0].url);

    announceAccessibleMessage(this.i18n('photoDiscardAccessibleText'));
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {string}
   * @private
   */
  getImageSrc_: function(selectedItem) {
    return (selectedItem && selectedItem.src) || '';
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {string}
   * @private
   */
  getImageType_: function(selectedItem) {
    return (selectedItem && selectedItem.dataset.type) ||
        CrPicture.SelectionTypes.NONE;
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {boolean} True if the author credit text is shown.
   * @private
   */
  isAuthorCreditShown_: function(selectedItem) {
    return !!selectedItem &&
        selectedItem.dataset.type == CrPicture.SelectionTypes.DEFAULT;
  },

  /**
   * @param {!CrPicture.ImageElement} selectedItem
   * @param {!Array<!settings.DefaultImage>} defaultImages
   * @return {string} The author name for the selected default image. An empty
   *     string is returned if there is no valid author name.
   * @private
   */
  getAuthorName_: function(selectedItem, defaultImages) {
    if (!this.isAuthorCreditShown_(selectedItem))
      return '';

    assert(
        selectedItem.dataset.index !== null &&
        selectedItem.dataset.index < defaultImages.length);
    return defaultImages[selectedItem.dataset.index].author;
  },

  /**
   * @param {!CrPicture.ImageElement} selectedItem
   * @param {!Array<!settings.DefaultImage>} defaultImages
   * @return {string} The author website for the selected default image. An
   *     empty string is returned if there is no valid author name.
   * @private
   */
  getAuthorWebsite_: function(selectedItem, defaultImages) {
    if (!this.isAuthorCreditShown_(selectedItem))
      return '';

    assert(
        selectedItem.dataset.index !== null &&
        selectedItem.dataset.index < defaultImages.length);
    return defaultImages[selectedItem.dataset.index].website;
  },
});
