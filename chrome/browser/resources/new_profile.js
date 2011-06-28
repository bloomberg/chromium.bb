// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var gSelectedAvatarIconIndex = 0;

///////////////////////////////////////////////////////////////////////////////
// Helper functions
function $(o) {return document.getElementById(o);}

function load() {
  chrome.send('requestProfileInfo', []);
  updateLogo();
}

function onCreate() {
  chrome.send('create', [$('profile-name').value,
                         String(gSelectedAvatarIconIndex)]);
}

function onCancel() {
  chrome.send('cancel', []);
}

function onAvatarClicked(index) {
  var menu = document.getElementById("avatar-menu");
  for (var i = 0; i < menu.childNodes.length; i++) {
    var button = menu.childNodes[i];
    if (i == index) {
      button.setAttribute("style", "background-color: #bbcee9");
    } else {
      button.setAttribute("style", "background-color: transparent");
    }
  }
  gSelectedAvatarIconIndex = index;
}

function updateLogo() {
  var imageId = 'IDR_PRODUCT_LOGO';
  if (document.documentElement.getAttribute('customlogo') == 'true')
    imageId = 'IDR_CUSTOM_PRODUCT_LOGO';

  $('logo-img').src = 'chrome://theme/' + imageId + '?' + Date.now();
}

///////////////////////////////////////////////////////////////////////////////
// Chrome callbacks:

function setProfileInfo(profileName, profileIconIndex) {
  $('profile-name').value = profileName;
  onAvatarClicked(profileIconIndex);
}

function setDefaultAvatarImages(imageUrlList) {
  var menu = document.getElementById("avatar-menu");
  for (var i = 0; i < imageUrlList.length; i++) {
    var button = document.createElement("input");
    button.setAttribute("type", "image");
    button.setAttribute("class", "avatar-button");
    button.setAttribute("src", imageUrlList[i]);
    button.setAttribute("onclick", "onAvatarClicked(" + i + ")");
    menu.appendChild(button);
  }
}

// Add handlers to HTML elements.
document.body.onload = load;
$('create-button').onclick = function () { onCreate(''); };
$('cancel-button').onclick = function () { onCancel(''); };
$('profile-name-form').onsubmit = function () {
  onCreate('');
  return false;
};
