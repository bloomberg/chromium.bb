// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe user image screen implementation.
 */

cr.define('oobe', function() {
  var UserImagesGrid = options.UserImagesGrid;
  var ButtonImages = UserImagesGrid.ButtonImages;

  /**
   * Array of button URLs used on this page.
   * @type {Array.<string>}
   * @const
   */
  var ButtonImageUrls = [
    ButtonImages.TAKE_PHOTO
  ];

  /**
   * Creates a new OOBE screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserImageScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  UserImageScreen.register = function() {
    var screen = $('user-image');
    var isWebRTC = document.documentElement.getAttribute('camera') == 'webrtc';
    UserImageScreen.prototype = isWebRTC ? UserImageScreenWebRTCProto :
        UserImageScreenOldProto;
    UserImageScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  var UserImageScreenOldProto = {
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

      imageGrid.addEventListener('select',
                                 this.handleSelect_.bind(this));
      imageGrid.addEventListener('activate',
                                 this.handleImageActivated_.bind(this));

      // Whether a button image is selected.
      this.buttonImageSelected_ = false;

      // Photo image data (if present).
      this.photoImage_ = null;

      // Profile image data (if present).
      this.profileImage_ = imageGrid.addItem(
          ButtonImages.PROFILE_PICTURE,
          undefined, undefined, undefined,
          function(el) {  // Custom decorator for Profile image element.
            var spinner = el.ownerDocument.createElement('div');
            spinner.className = 'spinner';
            var spinnerBg = el.ownerDocument.createElement('div');
            spinnerBg.className = 'spinner-bg';
            spinnerBg.appendChild(spinner);
            el.appendChild(spinnerBg);
            el.id = 'profile-image';
          });
      this.profileImageUrl_ = this.profileImage_.url;
      // True if a non-default profile image has been successfully loaded.
      this.profileImagePresent_ = false;

      // Initialize profile image state.
      this.profileImageSelected = false;
      this.profileImageLoading = true;
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
      okButton.addEventListener('click', this.acceptImage_.bind(this));
      return [okButton];
    },

    /**
     * The caption to use for the Profile image preview.
     * @type {string}
     */
    get profileImageCaption() {
      return this.profileImageCaption_;
    },
    set profileImageCaption(value) {
      this.profileImageCaption_ = value;
      this.updateCaption_();
    },

    /**
     * True if the Profile image is being loaded.
     * @type {boolean}
     */
    get profileImageLoading() {
      return this.profileImageLoading_;
    },
    set profileImageLoading(value) {
      this.profileImageLoading_ = value;
      $('user-image-screen-main').classList[
          value ? 'add' : 'remove']('profile-image-loading');
      this.updateProfileImageCaption_();
    },

    /**
     * True when a default image is selected (including button images).
     * @type {boolean}
     */
    set defaultImageSelected(value) {
      $('user-image-preview').classList[
          value ? 'add' : 'remove']('default-image');
    },

    /**
     * True when the profile image is selected.
     * @type {boolean}
     */
    get profileImageSelected() {
      return this.profileImageSelected_;
    },
    set profileImageSelected(value) {
      this.profileImageSelected_ = value;
      this.updateCaption_();
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
        default:
          this.acceptImage_();
          break;
      }
    },

    /**
     * Handles selection change.
     * @private
     */
    handleSelect_: function() {
      var selectedItem = $('user-image-grid').selectedItem;
      if (selectedItem === null)
        return;

      // Update preview image URL.
      var url = selectedItem.url;
      $('user-image-preview-img').src = url;

      // Update current selection type.
      this.defaultImageSelected = /^chrome:\/\/theme\//.test(url);
      // Cannot compare this.profileImage_ itself because it is updated
      // by setProfileImage_ after the selection event is fired programmaticaly.
      this.profileImageSelected = url == this.profileImageUrl_;
      this.buttonImageSelected_ = ButtonImageUrls.indexOf(url) != -1;

      if (ButtonImageUrls.indexOf(url) == -1) {
        // Non-button image is selected.
        $('ok-button').disabled = false;
        chrome.send('selectImage', [url]);
      } else {
        $('ok-button').disabled = true;
      }
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      Oobe.getInstance().headerHidden = true;
      $('user-image-grid').updateAndFocus();
      chrome.send('onUserImageScreenShown');
    },

    /**
     * Accepts currently selected image, if possible.
     * @private
     */
    acceptImage_: function() {
      var okButton = $('ok-button');
      if (!okButton.disabled) {
        // This ensures that #ok-button won't be re-enabled again.
        $('user-image-grid').disabled = true;
        okButton.disabled = true;
        chrome.send('onUserImageAccepted');
      }
    },

    /**
     * Notifies about camera presence change.
     * @param {boolean} present Whether a camera is present or not.
     * @private
     */
    setCameraPresent_: function(present) {
      var imageGrid = $('user-image-grid');
      if (present && !this.takePhotoButton_) {
        this.takePhotoButton_ = imageGrid.addItem(
            ButtonImages.TAKE_PHOTO,
            undefined,
            this.handleTakePhoto_.bind(this),
            0);
      } else if (!present && this.takePhotoButton_) {
        imageGrid.removeItem(this.takePhotoButton_);
        this.takePhotoButton_ = null;
      }
    },

    /**
     * Adds or updates image with user photo and sets it as preview.
     * @param {string} photoUrl Image encoded as data URL.
     * @private
     */
    setUserPhoto_: function(photoUrl) {
      var imageGrid = $('user-image-grid');
      if (this.photoImage_)
        this.photoImage_ = imageGrid.updateItem(this.photoImage_, photoUrl);
      else
        this.photoImage_ = imageGrid.addItem(photoUrl, undefined, undefined, 1);
      imageGrid.selectedItem = this.photoImage_;
      imageGrid.focus();
    },

    /**
     * Updates user profile image.
     * @param {?string} imageUrl Image encoded as data URL. If null, user has
     *     the default profile image, which we don't want to show.
     * @private
     */
    setProfileImage_: function(imageUrl) {
      this.profileImageLoading = false;
      if (imageUrl !== null) {
        this.profileImagePresent_ = true;
        this.profileImageUrl_ = imageUrl;
        this.profileImage_ =
            $('user-image-grid').updateItem(this.profileImage_, imageUrl);
      }
    },

    /**
     * Appends received images to the list.
     * @param {Array.<string>} images An array of URLs to user images.
     * @private
     */
    setUserImages_: function(images) {
      var imageGrid = $('user-image-grid');
      for (var i = 0, url; url = images[i]; i++)
        imageGrid.addItem(url);
    },

    /**
     * Selects user image with the given URL.
     * @param {string} url URL of the image to select.
     * @private
     */
    setSelectedImage_: function(url) {
      var imageGrid = $('user-image-grid');
      imageGrid.selectedItemUrl = url;
      imageGrid.focus();
    },

    /**
     * Updates the image preview caption.
     * @private
     */
    updateCaption_: function() {
      $('user-image-preview-caption').textContent =
          this.profileImageSelected ? this.profileImageCaption : '';
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      this.updateProfileImageCaption_();
    },

    /**
     * Updates profile image caption.
     * @private
     */
    updateProfileImageCaption_: function() {
      this.profileImageCaption = localStrings.getString(
        this.profileImageLoading_ ? 'profilePhotoLoading' : 'profilePhoto');
    }
  };

  var UserImageScreenWebRTCProto = {
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

      // Preview image will track the selected item's URL.
      imageGrid.previewElement = $('user-image-preview');
      imageGrid.selectionType = 'default';

      imageGrid.addEventListener('select',
                                 this.handleSelect_.bind(this));
      imageGrid.addEventListener('activate',
                                 this.handleImageActivated_.bind(this));

      // Profile image data (if present).
      this.profileImage_ = imageGrid.addItem(
          ButtonImages.PROFILE_PICTURE,
          undefined, undefined, undefined,
          function(el) {  // Custom decorator for Profile image element.
            var spinner = el.ownerDocument.createElement('div');
            spinner.className = 'spinner';
            var spinnerBg = el.ownerDocument.createElement('div');
            spinnerBg.className = 'spinner-bg';
            spinnerBg.appendChild(spinner);
            el.appendChild(spinnerBg);
            el.id = 'profile-image';
          });
      this.profileImage_.type = 'profile';

      // Add camera stream element.
      imageGrid.cameraImage = null;

      // Perform an early check if camera is present, without starting capture.
      imageGrid.checkCameraPresence(false, false);

      $('take-photo').addEventListener(
          'click', this.handleTakePhoto_.bind(this));
      $('discard-photo').addEventListener(
          'click', imageGrid.discardPhoto.bind(imageGrid));
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
      okButton.addEventListener('click', this.acceptImage_.bind(this));
      return [okButton];
    },

    /**
     * The caption to use for the Profile image preview.
     * @type {string}
     */
    get profileImageCaption() {
      return this.profileImageCaption_;
    },
    set profileImageCaption(value) {
      this.profileImageCaption_ = value;
      this.updateCaption_();
    },

    /**
     * True if the Profile image is being loaded.
     * @type {boolean}
     */
    get profileImageLoading() {
      return this.profileImageLoading_;
    },
    set profileImageLoading(value) {
      this.profileImageLoading_ = value;
      $('user-image-screen-main').classList[
          value ? 'add' : 'remove']('profile-image-loading');
      this.updateProfileImageCaption_();
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
        default:
          this.acceptImage_();
          break;
      }
    },

    /**
     * Handles selection change.
     * @private
     */
    handleSelect_: function() {
      if (this.selectionType == 'camera' && this.cameraLive) {
        // No current image selected.
        $('ok-button').disabled = true;
      } else {
        $('ok-button').disabled = false;
        chrome.send('selectImage', [$('user-image-grid').selectedItemUrl]);
      }
      this.updateCaption_();
    },

    /**
     * Handle photo capture from the live camera stream.
     */
    handleTakePhoto_: function(e) {
      $('user-image-grid').takePhoto(function(photoURL) {
        chrome.send('photoTaken', [photoURL]);
      });
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      Oobe.getInstance().headerHidden = true;
      var imageGrid = $('user-image-grid');
      imageGrid.updateAndFocus();
      // Now check again for camera presence and start capture.
      imageGrid.checkCameraPresence(true, true);
      chrome.send('onUserImageScreenShown');
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
    onBeforeHide: function() {
      $('user-image-grid').stopCamera();
    },

    /**
     * Accepts currently selected image, if possible.
     * @private
     */
    acceptImage_: function() {
      var okButton = $('ok-button');
      if (!okButton.disabled) {
        // This ensures that #ok-button won't be re-enabled again.
        $('user-image-grid').disabled = true;
        okButton.disabled = true;
        chrome.send('onUserImageAccepted');
      }
    },

    /**
     * Updates user profile image.
     * @param {?string} imageUrl Image encoded as data URL. If null, user has
     *     the default profile image, which we don't want to show.
     * @private
     */
    setProfileImage_: function(imageUrl) {
      this.profileImageLoading = false;
      if (imageUrl !== null) {
        this.profileImage_ =
            $('user-image-grid').updateItem(this.profileImage_, imageUrl);
      }
    },

    /**
     * Appends received images to the list.
     * @param {Array.<string>} images An array of URLs to user images.
     * @private
     */
    setUserImages_: function(images) {
      var imageGrid = $('user-image-grid');
      for (var i = 0, url; url = images[i]; i++)
        imageGrid.addItem(url).type = 'default';
    },

    /**
     * Selects user image with the given URL.
     * @param {string} url URL of the image to select.
     * @private
     */
    setSelectedImage_: function(url) {
      var imageGrid = $('user-image-grid');
      imageGrid.selectedItemUrl = url;
      imageGrid.focus();
    },

    /**
     * Updates the image preview caption.
     * @private
     */
    updateCaption_: function() {
      $('user-image-preview-caption').textContent =
          (this.selectionType == 'profile') ? this.profileImageCaption : '';
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      this.updateProfileImageCaption_();
    },

    /**
     * Updates profile image caption.
     * @private
     */
    updateProfileImageCaption_: function() {
      this.profileImageCaption = localStrings.getString(
        this.profileImageLoading_ ? 'profilePhotoLoading' : 'profilePhoto');
    }
  };

  // Forward public APIs to private implementations.
  [
    'setCameraPresent',
    'setProfileImage',
    'setSelectedImage',
    'setUserImages',
    'setUserPhoto',
  ].forEach(function(name) {
    UserImageScreen[name] = function(value) {
      $('user-image')[name + '_'](value);
    };
  });

  return {
    UserImageScreen: UserImageScreen
  };
});
