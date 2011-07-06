// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define("newProfile", function() {
  'use strict';

  // Tracks the currently selected avatar icon.
  var selectedAvatarIconIndex = 0;

  // Initializes everything once the document loads.
  function load() {
    // Allow platform specific CSS rules.
    cr.enablePlatformSpecificCSSRules();

    // Add handlers to HTML elements.
    $('create-button').onclick = onCreate;
    $('cancel-button').onclick = onCancel;
    $('profile-name-form').onsubmit = function () {
      onCreate();
      // Return false to prevent the submit handler from doing a post.
      return false;
    };

    chrome.send('requestProfileInfo');
    updateLogo();
    $('profile-name').focus();
  }

  // Sends the profile information to the browser.
  function onCreate() {
    chrome.send('create', [$('profile-name').value,
                           String(selectedAvatarIconIndex)]);
  }

  // Lets the browser know that the user doesn't want to create the profile.
  function onCancel() {
    chrome.send('cancel');
  }

  // Changes the selected profile.
  function onAvatarClicked(index) {
    var menu = $('avatar-menu');
    for (var i = 0; i < menu.childNodes.length; i++) {
      var button = menu.childNodes[i];
      if (i == index)
        button.classList.add("avatar-button-selected");
      else
        button.classList.remove("avatar-button-selected");
    }
    selectedAvatarIconIndex = index;
  }

  // Sets the logo image.
  function updateLogo() {
    var imageId = 'IDR_PRODUCT_LOGO';
    if (document.documentElement.getAttribute('customlogo') == 'true')
      imageId = 'IDR_CUSTOM_PRODUCT_LOGO';

    $('logo-img').src = 'chrome://theme/' + imageId + '?' + Date.now();
  }

  // Callback from the browser to set the profile information on the page.
  function setProfileInfo(profileName, profileIconIndex) {
    $('profile-name').value = profileName;
    onAvatarClicked(profileIconIndex);
  }

  // Callback from the browser to fill the avatar menu with default avatar
  // images.
  function setDefaultAvatarImages(imageUrlList) {
    var menu = $('avatar-menu');
    for (var i = 0; i < imageUrlList.length; i++) {
      var button = document.createElement("input");
      button.setAttribute("type", "image");
      button.setAttribute("class", "avatar-button");
      button.setAttribute("src", imageUrlList[i]);
      button.setAttribute("onclick", "newProfile.onAvatarClicked(" + i + ")");
      menu.appendChild(button);
    }
  }

  // Return an object with all the exports.
  return {
    load: load,
    onCreate: onCreate,
    onCancel: onCancel,
    onAvatarClicked: onAvatarClicked,
    updateLogo: updateLogo,
    setProfileInfo: setProfileInfo,
    setDefaultAvatarImages: setDefaultAvatarImages,
  };
});

window.addEventListener('DOMContentLoaded', newProfile.load);
