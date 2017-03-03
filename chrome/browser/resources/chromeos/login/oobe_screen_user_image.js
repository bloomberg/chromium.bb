// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe user image screen implementation.
 */

login.createScreen('UserImageScreen', 'user-image', function() {
  var CONTEXT_KEY_IS_CAMERA_PRESENT = 'isCameraPresent';
  var CONTEXT_KEY_SELECTED_IMAGE_URL = 'selectedImageURL';
  var CONTEXT_KEY_PROFILE_PICTURE_DATA_URL = 'profilePictureDataURL';

  var UserImagesGrid = options.UserImagesGrid;
  var ButtonImages = UserImagesGrid.ButtonImages;

  return {
    EXTERNAL_API: [
      'setDefaultImages',
      'hideCurtain'
    ],

    /** @override */
    decorate: function(element) {
      var imageGrid = $('user-image-grid');
      UserImagesGrid.decorate(imageGrid);

      // Preview image will track the selected item's URL.
      var previewElement = $('user-image-preview');
      previewElement.oncontextmenu = function(e) { e.preventDefault(); };

      imageGrid.previewElement = previewElement;
      imageGrid.selectionType = 'default';
      imageGrid.flipPhotoElement = $('flip-photo');

      imageGrid.addEventListener('select',
                                 this.handleSelect_.bind(this));
      imageGrid.addEventListener('activate',
                                 this.handleImageActivated_.bind(this));
      imageGrid.addEventListener('phototaken',
                                 this.handlePhotoTaken_.bind(this));
      imageGrid.addEventListener('photoupdated',
                                 this.handlePhotoUpdated_.bind(this));

      // Set the title for camera item in the grid.
      imageGrid.setCameraTitles(
          loadTimeData.getString('takePhoto'),
          loadTimeData.getString('photoFromCamera'));

      this.profileImageLoading = true;

      // Profile image data (if present).
      this.profileImage_ = imageGrid.addItem(
          ButtonImages.PROFILE_PICTURE,           // Image URL.
          loadTimeData.getString('profilePhoto'), // Title.
          undefined,                              // Click handler.
          0,                                      // Position.
          function(el) {
            // Custom decorator for Profile image element.
            var spinner = el.ownerDocument.createElement('div');
            spinner.className = 'spinner';
            var spinnerBg = el.ownerDocument.createElement('div');
            spinnerBg.className = 'spinner-bg';
            spinnerBg.appendChild(spinner);
            el.appendChild(spinnerBg);
            el.id = 'profile-image';
          });
      this.profileImage_.type = 'profile';

      $('take-photo').addEventListener(
          'click', this.handleTakePhoto_.bind(this));
      $('discard-photo').addEventListener(
          'click', this.handleDiscardPhoto_.bind(this));

      // Toggle 'animation' class for the duration of WebKit transition.
      $('flip-photo').addEventListener(
          'click', this.handleFlipPhoto_.bind(this));
      $('user-image-stream-crop').addEventListener(
          'transitionend', function(e) {
            previewElement.classList.remove('animation');
          });
      $('user-image-preview-img').addEventListener(
          'transitionend', function(e) {
            previewElement.classList.remove('animation');
          });

      var self = this;
      this.context.addObserver(CONTEXT_KEY_IS_CAMERA_PRESENT,
                               function(present) {
        $('user-image-grid').cameraPresent = present;
      });
      this.context.addObserver(CONTEXT_KEY_SELECTED_IMAGE_URL,
                               this.setSelectedImage_);
      this.context.addObserver(CONTEXT_KEY_PROFILE_PICTURE_DATA_URL,
                               function(url) {
        self.profileImageLoading = false;
        if (url) {
          self.profileImage_ =
              $('user-image-grid').updateItem(self.profileImage_, url);
        }
      });

      this.updateLocalizedContent();

      chrome.send('getImages');
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('userImageScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var okButton = this.ownerDocument.createElement('button');
      okButton.id = 'ok-button';
      okButton.textContent = loadTimeData.getString('okButtonText');
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
      $('user-image-screen-main').classList.toggle('profile-image-loading',
                                                   value);
      if (value)
        announceAccessibleMessage(loadTimeData.getString('syncingPreferences'));
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
     * @param {Event} e Selection change event.
     * @private
     */
    handleSelect_: function(e) {
      var imageGrid = $('user-image-grid');
      $('ok-button').disabled = false;

      // Camera selection
      if (imageGrid.selectionType == 'camera') {
        $('flip-photo').tabIndex = 1;
        // No current image selected.
        if (imageGrid.cameraLive) {
          imageGrid.previewElement.classList.remove('phototaken');
          $('ok-button').disabled = true;
        } else {
          imageGrid.previewElement.classList.add('phototaken');
          this.notifyImageSelected_();
        }
      } else {
        imageGrid.previewElement.classList.remove('phototaken');
        $('flip-photo').tabIndex = -1;
        this.notifyImageSelected_();
      }
      // Start/stop camera on (de)selection.
      if (!imageGrid.inProgramSelection &&
          imageGrid.selectionType != e.oldSelectionType) {
        if (imageGrid.selectionType == 'camera') {
          // Programmatic selection of camera item is done in
          // startCamera callback where streaming is started by itself.
          imageGrid.startCamera(
              function() {
                // Start capture if camera is still the selected item.
                $('user-image-preview-img').classList.toggle(
                    'animated-transform', true);
                return imageGrid.selectedItem == imageGrid.cameraImage;
              });
        } else {
          $('user-image-preview-img').classList.toggle('animated-transform',
                                                       false);
          imageGrid.stopCamera();
        }
      }
      this.updateCaption_();
      // Update image attribution text.
      var image = imageGrid.selectedItem;
      $('user-image-author-name').textContent = image.author;
      $('user-image-author-website').textContent = image.website;
      $('user-image-author-website').href = image.website;
      $('user-image-attribution').style.visibility =
          (image.author || image.website) ? 'visible' : 'hidden';
    },

    /**
     * Handle camera-photo flip.
     */
    handleFlipPhoto_: function() {
      var imageGrid = $('user-image-grid');
      imageGrid.previewElement.classList.add('animation');
      imageGrid.flipPhoto = !imageGrid.flipPhoto;
      var flipMessageId = imageGrid.flipPhoto ?
         'photoFlippedAccessibleText' : 'photoFlippedBackAccessibleText';
      announceAccessibleMessage(loadTimeData.getString(flipMessageId));
    },

    /**
     * Handle photo capture from the live camera stream.
     */
    handleTakePhoto_: function(e) {
      $('user-image-grid').takePhoto();
      chrome.send('takePhoto');
    },

    /**
     * Handle photo captured event.
     * @param {Event} e Event with 'dataURL' property containing a data URL.
     */
    handlePhotoTaken_: function(e) {
      chrome.send('photoTaken', [e.dataURL]);
      announceAccessibleMessage(
          loadTimeData.getString('photoCaptureAccessibleText'));
    },

    /**
     * Handle photo updated event.
     * @param {Event} e Event with 'dataURL' property containing a data URL.
     */
    handlePhotoUpdated_: function(e) {
      chrome.send('photoTaken', [e.dataURL]);
    },

    /**
     * Handle discarding the captured photo.
     */
    handleDiscardPhoto_: function(e) {
      var imageGrid = $('user-image-grid');
      imageGrid.discardPhoto();
      chrome.send('discardPhoto');
      announceAccessibleMessage(
          loadTimeData.getString('photoDiscardAccessibleText'));
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      Oobe.getInstance().headerHidden = true;
      this.loading = true;
      var imageGrid = $('user-image-grid');
      imageGrid.updateAndFocus();
      chrome.send('onUserImageScreenShown');
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
    onBeforeHide: function() {
      $('user-image-grid').stopCamera();
      this.loading = false;
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
        this.loading = true;
      }
    },

    /**
     * Appends default images to the image grid. Should only be called once.
     * @param {Array<{url: string, author: string, website: string}>} images
     *   An array of default images data, including URL, author and website.
     */
    setDefaultImages: function(imagesData) {
      $('user-image-grid').setDefaultImages(imagesData);
      this.setSelectedImage_(
          this.context.get(CONTEXT_KEY_SELECTED_IMAGE_URL, ''));
      chrome.send('screenReady');
    },

    /**
     * Selects user image with the given URL.
     * @param {string} url URL of the image to select.
     * @private
     */
    setSelectedImage_: function(url) {
      if (!url)
        return;
      var imageGrid = $('user-image-grid');
      imageGrid.selectedItemUrl = url;
      imageGrid.focus();
    },

    get loading() {
      return this.classList.contains('loading');
    },

    set loading(value) {
      this.classList.toggle('loading', value);
      $('oobe').classList.toggle('image-loading', value);
      Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Hides curtain with spinner.
     */
    hideCurtain: function() {
      this.loading = false;
    },

    /**
     * Updates the image preview caption.
     * @private
     */
    updateCaption_: function() {
      $('user-image-preview-caption').textContent =
          $('user-image-grid').selectionType == 'profile' ?
          this.profileImageCaption : '';
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
      this.profileImageCaption = loadTimeData.getString(
        this.profileImageLoading_ ? 'profilePhotoLoading' : 'profilePhoto');
    },

    /**
     * Notifies chrome about image selection.
     * @private
     */
    notifyImageSelected_: function() {
      var imageGrid = $('user-image-grid');
      chrome.send('selectImage',
                  [imageGrid.selectedItemUrl,
                   imageGrid.selectionType,
                   !imageGrid.inProgramSelection]);
    }
  };
});
