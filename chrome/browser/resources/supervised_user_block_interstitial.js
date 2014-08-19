// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sendCommand(cmd) {
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
}

function makeImageSet(url1x, url2x) {
  return '-webkit-image-set(url(' + url1x + ') 1x, url(' + url2x + ') 2x)';
}

function initialize() {
  if (loadTimeData.getBoolean('allowAccessRequests')) {
    $('request-access-button').onclick = function(event) {
      updateAfterRequestSent();
      sendCommand('request');
    };
  } else {
    $('request-access-button').hidden = true;
  }
  var avatarURL1x = loadTimeData.getString('avatarURL1x');
  var avatarURL2x = loadTimeData.getString('avatarURL2x');
  if (avatarURL1x) {
    $('avatar-img').style.content = makeImageSet(avatarURL1x, avatarURL2x);
    $('avatar-img').hidden = false;
    $('error-img').hidden = true;
    var secondAvatarURL1x = loadTimeData.getString('secondAvatarURL1x');
    var secondAvatarURL2x = loadTimeData.getString('secondAvatarURL2x');
    if (secondAvatarURL1x) {
      $('second-avatar-img').style.content =
          makeImageSet(secondAvatarURL1x, secondAvatarURL2x);
      $('second-avatar-img').hidden = false;
      // The avatar images should overlap a bit.
      $('avatar-img').style.left = '10px';
      $('avatar-img').style.zIndex = '1';
      $('second-avatar-img').style.left = '-10px';
    }
  }
  $('back-button').onclick = function(event) {
    sendCommand('back');
  };
}

/**
 * Updates the interstitial to show that the request was sent.
 */
function updateAfterRequestSent() {
  $('error-img').hidden = true;
  $('request-access-button').hidden = true;
  $('block-page-message').hidden = true;
  $('request-sent-message').hidden = false;
  if ($('avatar-img').hidden) {
    $('request-sent-message').style.marginTop = '40px';
  }
}

document.addEventListener('DOMContentLoaded', initialize);
