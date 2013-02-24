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
   * Whether the web camera item should be preselected, if available.
   * @type {boolean}
   * @const
   */
  var PRESELECT_CAMERA = false;

  /**
   * Creates a new OOBE screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserImageScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   * @param {boolean} lazyInit If true, screen is decorated on first show.
   */
  UserImageScreen.register = function(lazyInit) {
    var screen = $('user-image');
    if (lazyInit) {
      screen.__proto__ = UserImageScreen.prototype;
      screen.deferredDecorate = function() {
        UserImageScreen.decorate(screen);
      };
    } else {
      UserImageScreen.decorate(screen);
    }
    Oobe.getInstance().registerScreen(screen);
  };

  UserImageScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Currently selected user image index (take photo button is with zero
     * index).
     * @type {number}
     */
    selectedUserImage_: -1,

    /** @override */
    decorate: function(element) {
      var imageGrid = $('user-image-grid');
      UserImagesGrid.decorate(imageGrid);

      // Preview image will track the selected item's URL.
      var previewElement = $('user-image-preview');
      imageGrid.previewElement = previewElement;
      imageGrid.selectionType = 'default';

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

      // Profile image data (if present).
      this.profileImage_ = imageGrid.addItem(
          ButtonImages.PROFILE_PICTURE,  // Image URL.
          loadTimeData.getString('profilePhoto'),  // Title.
          undefined,  // Click handler.
          undefined,  // Position.
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
      this.profileImageLoading = true;

      $('take-photo').addEventListener(
          'click', this.handleTakePhoto_.bind(this));
      $('discard-photo').addEventListener(
          'click', this.handleDiscardPhoto_.bind(this));

      // Toggle 'animation' class for the duration of WebKit transition.
      $('flip-photo').addEventListener(
          'click', function(e) {
            previewElement.classList.add('animation');
            imageGrid.flipPhoto = !imageGrid.flipPhoto;
          });
      $('user-image-stream-crop').addEventListener(
          'webkitTransitionEnd', function(e) {
            previewElement.classList.remove('animation');
          });
      $('user-image-preview-img').addEventListener(
          'webkitTransitionEnd', function(e) {
            previewElement.classList.remove('animation');
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
     * @param {cr.Event} e Selection change event.
     * @private
     */
    handleSelect_: function(e) {
      var imageGrid = $('user-image-grid');
      if (imageGrid.selectionType == 'camera' && imageGrid.cameraLive) {
        // No current image selected.
        $('ok-button').disabled = true;
      } else {
        $('ok-button').disabled = false;
        chrome.send('selectImage',
                    [imageGrid.selectedItemUrl, imageGrid.selectionType]);
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
                return imageGrid.selectedItem == imageGrid.cameraImage;
              });
        } else {
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
     * Handle photo capture from the live camera stream.
     */
    handleTakePhoto_: function(e) {
      $('user-image-grid').takePhoto();
    },

    /**
     * Handle photo captured event.
     * @param {cr.Event} e Event with 'dataURL' property containing a data URL.
     */
    handlePhotoTaken_: function(e) {
      chrome.send('photoTaken', [e.dataURL]);
      this.announceAccessibleMessage_(
          loadTimeData.getString('photoCaptureAccessibleText'));
    },

    /**
     * Handle photo updated event.
     * @param {cr.Event} e Event with 'dataURL' property containing a data URL.
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
      this.announceAccessibleMessage_(
          loadTimeData.getString('photoDiscardAccessibleText'));
    },

    /**
     * Add an accessible message to the page that will be announced to
     * users who have spoken feedback on, but will be invisible to all
     * other users. It's removed right away so it doesn't clutter the DOM.
     */
    announceAccessibleMessage_: function(msg) {
      var element = document.createElement('div');
      element.setAttribute('aria-live', 'polite');
      element.style.position = 'relative';
      element.style.left = '-9999px';
      element.style.height = '0px';
      element.innerText = msg;
      document.body.appendChild(element);
      window.setTimeout(function() {
        document.body.removeChild(element);
      }, 0);
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      Oobe.getInstance().headerHidden = true;
      var imageGrid = $('user-image-grid');
      imageGrid.updateAndFocus();
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
     * @param {boolean} present Whether camera is detected.
     */
    setCameraPresent_: function(present) {
      $('user-image-grid').cameraPresent = present;
    },

    /**
     * Appends default images to the image grid. Should only be called once.
     * @param {Array.<{url: string, author: string, website: string}>} images
     *   An array of default images data, including URL, author and website.
     * @private
     */
    setDefaultImages_: function(imagesData) {
      var imageGrid = $('user-image-grid');
      for (var i = 0, data; data = imagesData[i]; i++) {
        var item = imageGrid.addItem(data.url, data.title);
        item.type = 'default';
        item.author = data.author || '';
        item.website = data.website || '';
      }
      this.classList.remove('loading');
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
    }
  };

  // Forward public APIs to private implementations.
  [
    'setDefaultImages',
    'setCameraPresent',
    'setProfileImage',
    'setSelectedImage',
  ].forEach(function(name) {
    UserImageScreen[name] = function(value) {
      $('user-image')[name + '_'](value);
    };
  });

  return {
    UserImageScreen: UserImageScreen
  };
});
