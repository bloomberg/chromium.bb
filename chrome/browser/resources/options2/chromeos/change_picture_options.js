// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;
  var UserImagesGrid = options.UserImagesGrid;
  var ButtonImages = UserImagesGrid.ButtonImages;

  /**
   * Array of button URLs used on this page.
   * @type {Array.<string>}
   * @const
   */
  var ButtonImageUrls = [
    ButtonImages.TAKE_PHOTO,
    ButtonImages.CHOOSE_FILE
  ];

  /////////////////////////////////////////////////////////////////////////////
  // ChangePictureOptions class:

  /**
   * Encapsulated handling of ChromeOS change picture options page.
   * @constructor
   */
  function ChangePictureOptions() {
    var isWebRTC = $('change-picture-page').getAttribute('camera') == 'webrtc';
    ChangePictureOptions.prototype = isWebRTC ?
        ChangePictureOptionsWebRTCProto : ChangePictureOptionsOldProto;
    // |this| has been already created so it's |__proto__| has to be reset.
    this.__proto__ = ChangePictureOptions.prototype;
    OptionsPage.call(
        this,
        'changePicture',
        loadTimeData.getString('changePicturePage'),
        'change-picture-page');
  }

  cr.addSingletonGetter(ChangePictureOptions);

  var ChangePictureOptionsOldProto = {
    // Inherit ChangePictureOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    /**
     * Initializes ChangePictureOptions page.
     */
    initializePage: function() {
      // Call base class implementation to start preferences initialization.
      OptionsPage.prototype.initializePage.call(this);

      var imageGrid = $('user-image-grid');
      UserImagesGrid.decorate(imageGrid);

      imageGrid.addEventListener('select',
                                 this.handleImageSelected_.bind(this));
      imageGrid.addEventListener('activate',
                                 this.handleImageActivated_.bind(this));

      // Add the "Choose file" button.
      imageGrid.addItem(ButtonImages.CHOOSE_FILE,
                        loadTimeData.getString('chooseFile'),
                        this.handleChooseFile_.bind(this));

      // Profile image data.
      this.profileImage_ = imageGrid.addItem(
          ButtonImages.PROFILE_PICTURE,
          loadTimeData.getString('profilePhotoLoading'));

      // Old user image data (if present).
      this.oldImage_ = null;

      $('change-picture-overlay-confirm').onclick = this.closePage_;

      chrome.send('onChangePicturePageInitialized');
    },

    /**
     * Called right after the page has been shown to user.
     */
    didShowPage: function() {
      $('user-image-grid').updateAndFocus();
      chrome.send('onChangePicturePageShown');
    },

    /**
     * Called right before the page is hidden.
     */
    willHidePage: function() {
      var imageGrid = $('user-image-grid');
      imageGrid.blur();  // Make sure the image grid is not active.
      if (this.oldImage_) {
        imageGrid.removeItem(this.oldImage_);
        this.oldImage_ = null;
      }
    },

    /**
     * Called right after the page has been hidden.
     */
    // TODO(ivankr): both callbacks are required as only one of them is called
    // depending on the way the page was closed, see http://crbug.com/118923.
    didClosePage: function() {
      this.willHidePage();
    },

    /**
     * Closes current page, returning back to Personal Stuff page.
     * @private
     */
    closePage_: function() {
      OptionsPage.closeOverlay();
    },

    /**
     * Handles "Take photo" button activation.
     * @private
     */
    handleTakePhoto_: function() {
      chrome.send('takePhoto');
      this.closePage_();
    },

    /**
     * Handles "Choose a file" button activation.
     * @private
     */
    handleChooseFile_: function() {
      chrome.send('chooseFile');
      this.closePage_();
    },

    /**
     * Handles image selection change.
     * @private
     */
    handleImageSelected_: function() {
      var imageGrid = $('user-image-grid');
      var url = imageGrid.selectedItemUrl;
      // Ignore deselection, selection change caused by program itself and
      // selection of one of the action buttons.
      if (url &&
          !imageGrid.inProgramSelection &&
          ButtonImageUrls.indexOf(url) == -1) {
        chrome.send('selectImage', [url]);
      }
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
        case ButtonImages.CHOOSE_FILE:
          this.handleChooseFile_();
          break;
        default:
          this.closePage_();
          break;
      }
    },

    /**
     * URL of the current user image.
     * @type {string}
     */
    get currentUserImageUrl() {
      return 'chrome://userimage/' + BrowserOptions.getLoggedInUsername() +
          '?id=' + (new Date()).getTime() + '&animated';
    },

    /**
     * Notifies about camera presence change.
     * @param {boolean} present Whether a camera is present or not.
     * @private
     */
    setCameraPresent_: function(present) {
      var imageGrid = $('user-image-grid');
      var showTakePhotoButton = present;
      if (showTakePhotoButton && !this.takePhotoButton_) {
        this.takePhotoButton_ = imageGrid.addItem(
            ButtonImages.TAKE_PHOTO,
            loadTimeData.getString('takePhoto'),
            this.handleTakePhoto_.bind(this),
            1);
      } else if (!showTakePhotoButton && this.takePhotoButton_) {
        imageGrid.removeItem(this.takePhotoButton_);
        this.takePhotoButton_ = null;
      }
    },

    /**
     * Adds or updates old user image taken from file/camera (neither a profile
     * image nor a default one).
     * @private
     */
    setOldImage_: function() {
      var imageGrid = $('user-image-grid');
      var url = this.currentUserImageUrl;
      if (this.oldImage_) {
        this.oldImage_ = imageGrid.updateItem(this.oldImage_, url);
      } else {
        // Insert next to the profile image.
        var pos = imageGrid.indexOf(this.profileImage_) + 1;
        this.oldImage_ = imageGrid.addItem(url, undefined, undefined, pos);
        imageGrid.selectedItem = this.oldImage_;
      }
    },

    /**
     * Updates user's profile image.
     * @param {string} imageUrl Profile image, encoded as data URL.
     * @param {boolean} select If true, profile image should be selected.
     * @private
     */
    setProfileImage_: function(imageUrl, select) {
      var imageGrid = $('user-image-grid');
      this.profileImage_ = imageGrid.updateItem(
          this.profileImage_, imageUrl, loadTimeData.getString('profilePhoto'));
      if (select)
        imageGrid.selectedItem = this.profileImage_;
    },

    /**
     * Selects user image with the given URL.
     * @param {string} url URL of the image to select.
     * @private
     */
    setSelectedImage_: function(url) {
      $('user-image-grid').selectedItemUrl = url;
    },

    /**
     * Appends default images to the image grid. Should only be called once.
     * @param {Array.<string>} images An array of URLs to default images.
     * @private
     */
    setDefaultImages_: function(images) {
      var imageGrid = $('user-image-grid');
      for (var i = 0, url; url = images[i]; i++) {
        imageGrid.addItem(url);
      }
    },
  };

  var ChangePictureOptionsWebRTCProto = {
    // Inherit ChangePictureOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    /**
     * Initializes ChangePictureOptions page.
     */
    initializePage: function() {
      // Call base class implementation to start preferences initialization.
      OptionsPage.prototype.initializePage.call(this);

      var imageGrid = $('user-image-grid');
      UserImagesGrid.decorate(imageGrid);

      // Preview image will track the selected item's URL.
      imageGrid.previewElement = $('user-image-preview');
      imageGrid.selectionType = 'default';

      imageGrid.addEventListener('select',
                                 this.handleImageSelected_.bind(this));
      imageGrid.addEventListener('activate',
                                 this.handleImageActivated_.bind(this));

      // Set the title for "Take Photo" button.
      imageGrid.cameraTitle = loadTimeData.getString('takePhoto');

      // Add the "Choose file" button.
      imageGrid.addItem(ButtonImages.CHOOSE_FILE,
                        loadTimeData.getString('chooseFile'),
                        this.handleChooseFile_.bind(this)).type = 'file';

      // Profile image data.
      this.profileImage_ = imageGrid.addItem(
          ButtonImages.PROFILE_PICTURE,
          loadTimeData.getString('profilePhotoLoading'));
      this.profileImage_.type = 'profile';

      $('take-photo').addEventListener(
          'click', this.handleTakePhoto_.bind(this));
      $('discard-photo').addEventListener(
          'click', imageGrid.discardPhoto.bind(imageGrid));

      // Old user image data (if present).
      this.oldImage_ = null;

      $('change-picture-overlay-confirm').addEventListener(
          'click', this.closePage_.bind(this));

      chrome.send('onChangePicturePageInitialized');
    },

    /**
     * Called right after the page has been shown to user.
     */
    didShowPage: function() {
      var imageGrid = $('user-image-grid');
      imageGrid.updateAndFocus();
      // Reset camera element.
      imageGrid.cameraImage = null;
      // Autoplay but do not preselect.
      imageGrid.checkCameraPresence(true, false);
      chrome.send('onChangePicturePageShown');
    },

    /**
     * Called right before the page is hidden.
     */
    willHidePage: function() {
      var imageGrid = $('user-image-grid');
      imageGrid.blur();  // Make sure the image grid is not active.
      imageGrid.stopCamera();
      if (this.oldImage_) {
        imageGrid.removeItem(this.oldImage_);
        this.oldImage_ = null;
      }
    },

    /**
     * Called right after the page has been hidden.
     */
    // TODO(ivankr): both callbacks are required as only one of them is called
    // depending on the way the page was closed, see http://crbug.com/118923.
    didClosePage: function() {
      this.willHidePage();
    },

    /**
     * Closes current page, returning back to Personal Stuff page.
     * @private
     */
    closePage_: function() {
      OptionsPage.closeOverlay();
    },

    /**
     * Handles "Take photo" button activation.
     * @private
     */
    handleTakePhoto_: function() {
      $('user-image-grid').takePhoto(function(photoURL) {
        chrome.send('photoTaken', [photoURL]);
      });
    },

    /**
     * Handles "Choose a file" button activation.
     * @private
     */
    handleChooseFile_: function() {
      chrome.send('chooseFile');
      this.closePage_();
    },

    /**
     * Handles image selection change.
     * @private
     */
    handleImageSelected_: function() {
      var imageGrid = $('user-image-grid');
      var url = imageGrid.selectedItemUrl;
      // Ignore selection change caused by program itself and selection of one
      // of the action buttons.
      if (!imageGrid.inProgramSelection &&
          url != ButtonImages.TAKE_PHOTO && url != ButtonImages.CHOOSE_FILE) {
        chrome.send('selectImage', [url]);
      }
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
        case ButtonImages.CHOOSE_FILE:
          this.handleChooseFile_();
          break;
        default:
          this.closePage_();
          break;
      }
    },

    /**
     * URL of the current user image.
     * @type {string}
     */
    get currentUserImageUrl() {
      return 'chrome://userimage/' + BrowserOptions.getLoggedInUsername() +
          '?id=' + (new Date()).getTime() + '&animated';
    },

    /**
     * Adds or updates old user image taken from file/camera (neither a profile
     * image nor a default one).
     * @private
     */
    setOldImage_: function() {
      var imageGrid = $('user-image-grid');
      var url = this.currentUserImageUrl;
      if (this.oldImage_) {
        this.oldImage_ = imageGrid.updateItem(this.oldImage_, url);
      } else {
        // Insert next to the profile image.
        var pos = imageGrid.indexOf(this.profileImage_) + 1;
        this.oldImage_ = imageGrid.addItem(url, undefined, undefined, pos);
        imageGrid.selectedItem = this.oldImage_;
      }
    },

    /**
     * Updates user's profile image.
     * @param {string} imageUrl Profile image, encoded as data URL.
     * @param {boolean} select If true, profile image should be selected.
     * @private
     */
    setProfileImage_: function(imageUrl, select) {
      var imageGrid = $('user-image-grid');
      this.profileImage_ = imageGrid.updateItem(
          this.profileImage_, imageUrl, loadTimeData.getString('profilePhoto'));
      if (select)
        imageGrid.selectedItem = this.profileImage_;
    },

    /**
     * Selects user image with the given URL.
     * @param {string} url URL of the image to select.
     * @private
     */
    setSelectedImage_: function(url) {
      $('user-image-grid').selectedItemUrl = url;
    },

    /**
     * Appends default images to the image grid. Should only be called once.
     * @param {Array.<string>} images An array of URLs to default images.
     * @private
     */
    setDefaultImages_: function(images) {
      var imageGrid = $('user-image-grid');
      for (var i = 0, url; url = images[i]; i++) {
        imageGrid.addItem(url).type = 'default';
      }
    },
  };

  // Forward public APIs to private implementations.
  [
    'setCameraPresent',
    'setDefaultImages',
    'setOldImage',
    'setProfileImage',
    'setSelectedImage',
  ].forEach(function(name) {
    ChangePictureOptions[name] = function() {
      var instance = ChangePictureOptions.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ChangePictureOptions: ChangePictureOptions
  };

});
