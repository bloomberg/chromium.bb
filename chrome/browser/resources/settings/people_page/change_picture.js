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
    selectedItem_: {
      type: Object,
      value: null,
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
  },

  listeners: {
    'discard-image': 'onDiscardImage_',
    'image-activate': 'onImageActivate_',
    'photo-flipped': 'onPhotoFlipped_',
    'photo-taken': 'onPhotoTaken_',
  },

  /** @private {?settings.ChangePictureBrowserProxy} */
  browserProxy_: null,

  /** @private {?CrPictureListElement} */
  pictureList_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.ChangePictureBrowserProxyImpl.getInstance();
    this.pictureList_ =
        /** @type {CrPictureListElement} */ (this.$.pictureList);
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
      this.pictureList_.setFocus();
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
    this.pictureList_.setSelectedImageUrl(imageUrl);
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
    this.pictureList_.setOldImageUrl(imageUrl);
  },

  /**
   * Handler for the 'profile-image-changed' event.
   * @param {string} imageUrl
   * @param {boolean} selected
   * @private
   */
  receiveProfileImage_: function(imageUrl, selected) {
    this.pictureList_.setProfileImageUrl(imageUrl, selected);
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
        /** CrPicturePreviewElement */ (this.$.picturePreview).takePhoto();
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
   * Handler for when an image is activated.
   * @param {!{detail: !CrPicture.ImageElement}} event
   * @private
   */
  onImageActivate_: function(event) {
    this.selectImage_(event.detail);
  },

  /**
   * @param {!{detail: !{photoDataUrl: string}}} event
   * @private
   */
  onPhotoTaken_: function(event) {
    this.browserProxy_.photoTaken(event.detail.photoDataUrl);
    this.pictureList_.setFocus();
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
    this.pictureList_.setOldImageUrl('');

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
  getAuthorCredit_: function(selectedItem, defaultImages) {
    var index = selectedItem ? selectedItem.dataset.index : undefined;
    if (index === undefined)
      return '';
    assert(index < defaultImages.length);
    return this.i18n('authorCreditText', defaultImages[index].author);
  },

  /**
   * @param {!CrPicture.ImageElement} selectedItem
   * @param {!Array<!settings.DefaultImage>} defaultImages
   * @return {string} The author name for the selected default image. An empty
   *     string is returned if there is no valid author name.
   * @private
   */
  getAuthorWebsite_: function(selectedItem, defaultImages) {
    var index = selectedItem ? selectedItem.dataset.index : undefined;
    if (index === undefined)
      return '';
    assert(index < defaultImages.length);
    return defaultImages[index].website;
  },
});
