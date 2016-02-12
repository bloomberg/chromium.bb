// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * An object describing a default image.
 * @typedef {{
 *   author: string,
 *   title: string,
 *   url: string,
 *   website: string
 * }}
 */
settings.DefaultImage;

cr.define('settings', function() {
  /**
   * API which encapsulates messaging between JS and C++ for the ChromeOS
   * Change Picture subpage.
   * @constructor
   */
  function ChangePicturePrivateApi() {}

  /**
   * URLs of special button images.
   * @enum {string}
   */
  ChangePicturePrivateApi.ButtonImages = {
    TAKE_PHOTO: 'chrome://theme/IDR_BUTTON_USER_IMAGE_TAKE_PHOTO',
    CHOOSE_FILE: 'chrome://theme/IDR_BUTTON_USER_IMAGE_CHOOSE_FILE',
    PROFILE_PICTURE: 'chrome://theme/IDR_PROFILE_PICTURE_LOADING'
  };

  /**
   * Called from JavaScript. Retrieves the initial set of default images,
   * profile image, etc. As a response, the C++ calls these ChangePicturePage
   * methods as callbacks: receiveDefaultImages, receiveOldImage,
   * receiveProfileImage, and receiveSelectedImage.
   */
  ChangePicturePrivateApi.initialize = function() {
    chrome.send('onChangePicturePageInitialized');
  };

  /**
   * Called from JavaScript. Sets the user image to one of the default images.
   * As a response, the C++ calls ChangePicturePage.receiveSelectedImage.
   * @param {string} imageUrl
   */
  ChangePicturePrivateApi.selectDefaultImage = function(imageUrl) {
    chrome.send('selectImage', [imageUrl, 'default']);
  };

  /**
   * Called from JavaScript. Sets the user image to the 'old' image.
   * As a response, the C++ calls ChangePicturePage.receiveSelectedImage.
   */
  ChangePicturePrivateApi.selectOldImage = function() {
    chrome.send('selectImage', ['', 'old']);
  };

  /**
   * Called from JavaScript. Sets the user image to the profile image.
   * As a response, the C++ calls ChangePicturePage.receiveSelectedImage.
   */
  ChangePicturePrivateApi.selectProfileImage = function() {
    chrome.send('selectImage', ['', 'profile']);
  };

  /**
   * Called from JavaScript. Provides the taken photo as a data URL to the C++.
   * No response is expected.
   * @param {string} photoDataUrl
   */
  ChangePicturePrivateApi.photoTaken = function(photoDataUrl) {
    chrome.send('photoTaken', [photoDataUrl]);
  };

  return {
    ChangePicturePrivateApi: ChangePicturePrivateApi,
  };
});
