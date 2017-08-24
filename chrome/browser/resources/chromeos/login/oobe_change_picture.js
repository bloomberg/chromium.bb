// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An object describing a default image.
 * @typedef {{
 *   author: string,
 *   title: string,
 *   url: string,
 *   website: string
 * }}
 */
var OobeDefaultImage;

/** @const */ var OOBE_DEFAULT_AVATAR_IMAGE_URL =
    'chrome://theme/IDR_LOGIN_DEFAULT_USER';

/**
 * @fileoverview
 * 'oobe-change-picture' is a custom element containing controls to select
 * a user's picture.
 */
Polymer({
  is: 'oobe-change-picture',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * True if the user has a plugged-in webcam.
     * @private {boolean}
     */
    cameraPresent: {
      type: Boolean,
      value: false,
    },

    /** Specifies the selected image URL, used to specify the initial image. */
    selectedImageUrl: {
      type: String,
      observer: 'selectedImageUrlChanged_',
    },

    /**
     * The default user images.
     * @private {!Array<!OobeDefaultImage>}
     */
    defaultImages: {
      type: Array,
      value: function() {
        return [];
      },
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
  },

  listeners: {
    'discard-image': 'onDiscardImage_',
    'image-activate': 'onImageActivate_',
    'photo-flipped': 'onPhotoFlipped_',
    'photo-taken': 'onPhotoTaken_',
  },

  /** @private {?CrPictureListElement} */
  pictureList_: null,

  /** @override */
  ready: function() {
    this.pictureList_ =
        /** @type {CrPictureListElement} */ (this.$.pictureList);
  },

  /** Called when the screen is shown. */
  focus: function() {
    this.$.pictureList.setFocus();
  },

  /**
   * @return {string}
   * @private
   */
  getDescription_: function() {
    return this.i18nAdvanced(
        'userImageScreenDescription', {substitutions: [], tags: ['br']});
  },

  /**
   * selectedImageUrl is set by the host initially and possibly after sync.
   * @param {string} selectedImageUrl
   * @private
   */
  selectedImageUrlChanged_: function(selectedImageUrl) {
    var pictureList = /** @type {CrPictureListElement} */ (this.$.pictureList);
    pictureList.setSelectedImageUrl(selectedImageUrl);
    pictureList.setFocus();
  },

  /**
   * Selects an image element.
   * @param {!CrPicture.ImageElement} image
   * @private
   */
  selectImage_: function(image) {
    switch (image.dataset.type) {
      case CrPicture.SelectionTypes.CAMERA:
        this.sendSelectImage_(image.dataset.type, '');
        /** CrPicturePaneElement */ (this.$.picturePane).takePhoto();
        break;
      case CrPicture.SelectionTypes.FILE:
        assertNotReached();
        break;
      case CrPicture.SelectionTypes.PROFILE:
        this.sendSelectImage_(image.dataset.type, '');
        break;
      case CrPicture.SelectionTypes.OLD:
        this.sendSelectImage_(image.dataset.type, '');
        break;
      case CrPicture.SelectionTypes.DEFAULT:
        this.sendSelectImage_(image.dataset.type, image.dataset.url);
        break;
      default:
        assertNotReached('Selected unknown image type');
    }
  },

  /**
   * @param {string} type
   * @param {string} src
   * @private
   */
  sendSelectImage_: function(type, src) {
    chrome.send('selectImage', [type, src, true]);
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
    chrome.send('photoTaken', [event.detail.photoDataUrl]);
    this.pictureList_.setOldImageUrl(event.detail.photoDataUrl);

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

  /** @private */
  onDiscardImage_: function() {
    this.pictureList_.setOldImageUrl(CrPicture.kDefaultImageUrl);
    // Revert to profile image as we don't know what last used default image is.
    this.sendSelectImage_(CrPicture.SelectionTypes.PROFILE, '');
    chrome.send('discardPhoto');  // Plays 'SOUND_OBJECT_DELETE'.
    announceAccessibleMessage(this.i18n('photoDiscardAccessibleText'));
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {string}
   * @private
   */
  getImageSrc_: function(selectedItem) {
    return (selectedItem && selectedItem.dataset.url) || '';
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
});
