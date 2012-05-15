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
    OptionsPage.call(
        this,
        'changePicture',
        loadTimeData.getString('changePicturePage'),
        'change-picture-page');
  }

  cr.addSingletonGetter(ChangePictureOptions);

  ChangePictureOptions.prototype = {
    // Inherit ChangePictureOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    /**
     * Initializes ChangePictureOptions page.
     */
    initializePage: function() {
      // Call base class implementation to start preferences initialization.
      OptionsPage.prototype.initializePage.call(this);

      var imageGrid = $('images-grid');
      UserImagesGrid.decorate(imageGrid);

      imageGrid.addEventListener('change',
                                 this.handleImageSelected_.bind(this));
      imageGrid.addEventListener('activate',
                                 this.handleImageActivated_.bind(this));
      imageGrid.addEventListener('dblclick',
                                 this.handleImageDblClick_.bind(this));

      // Ephemeral users can choose from the standard pictures only. This is
      // because a custom image would have to be written to a file outside the
      // user's cryptohome where its removal on logout could not be guaranteed.
      if (!this.userIsEphemeral_()) {
        // Add the "Choose file" button.
        imageGrid.addItem(ButtonImages.CHOOSE_FILE,
                          loadTimeData.getString('chooseFile'),
                          this.handleChooseFile_.bind(this));

        // Profile image data.
        this.profileImage_ = imageGrid.addItem(
            ButtonImages.PROFILE_PICTURE,
            loadTimeData.getString('profilePhotoLoading'));
      }

      // Old user image data (if present).
      this.oldImage_ = null;

      $('change-picture-overlay-confirm').onclick = this.closePage_;

      chrome.send('onChangePicturePageInitialized');
    },

    /**
     * Called right after the page has been shown to user.
     */
    didShowPage: function() {
      $('images-grid').updateAndFocus();
      chrome.send('onChangePicturePageShown');
    },

    /**
     * Called right before the page is hidden.
     */
    willHidePage: function() {
      var imageGrid = $('images-grid');
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
      var imageGrid = $('images-grid');
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
      switch ($('images-grid').selectedItemUrl) {
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
     * Handles double click on the image grid.
     * @param {Event} e Double click Event.
     */
    handleImageDblClick_: function(e) {
      // Close page unless the click target is the grid itself or
      // any of the buttons.
      var url = e.target.src;
      if (url && ButtonImageUrls.indexOf(url) == -1)
        this.closePage_();
    },

    /**
     * URL of the current user image.
     * @type {string}
     */
    get currentUserImageUrl() {
      return 'chrome://userimage/' + BrowserOptions.getLoggedInUsername() +
          '?id=' + (new Date()).getTime();
    },

    /**
     * Notifies about camera presence change.
     * @param {boolean} present Whether a camera is present or not.
     * @private
     */
    setCameraPresent_: function(present) {
      var imageGrid = $('images-grid');
      // Ephemeral users can choose from the standard pictures only. This is
      // because a custom image would have to be written to a file outside the
      // user's cryptohome where its removal on logout could not be guaranteed.
      var showTakePhotoButton = present && !this.userIsEphemeral_();
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
      var imageGrid = $('images-grid');
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
      var imageGrid = $('images-grid');
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
      $('images-grid').selectedItemUrl = url;
    },

    /**
     * Appends default images to the image grid. Should only be called once.
     * @param {Array.<string>} images An array of URLs to default images.
     * @private
     */
    setDefaultImages_: function(images) {
      var imageGrid = $('images-grid');
      for (var i = 0, url; url = images[i]; i++) {
        imageGrid.addItem(url);
      }
    },

    /**
     * Returns whether the user is logged in as ephemeral.
     * @return {boolean} True if the user is logged in as ephemeral.
     * @private
     */
    userIsEphemeral_: function() {
      return loadTimeData.getBoolean('userIsEphemeral');
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

