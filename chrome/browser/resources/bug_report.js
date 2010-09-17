// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants.
var FEEDBACK_LANDING_PAGE =
    'http://www.google.com/support/chrome/go/feedback_confirmation'

var selectedThumbnailDivId = '';
var selectedThumbnailId = '';
var selectedImageUrl;

var savedThumbnailIds = [];
savedThumbnailIds['current-screenshots'] = '';
savedThumbnailIds['saved-screenshots'] = '';

var localStrings = new LocalStrings();

/**
 * Selects an image thumbnail in the specified div.
 */
function selectImage(divId, thumbnailId) {
  var thumbnailDivs = $(divId).children;
  selectedThumbnailDivId = divId;
  if (thumbnailDivs.length == 0) {
    $(divId).style.display = 'none';
    return;
  }
  for (var i = 0; i < thumbnailDivs.length; i++) {
    // If the the current div matches the thumbnail id provided,
    // or there is no thumbnail id given, and we're at the first thumbnail.
    if ((thumbnailDivs[i].id == thumbnailId) || (!thumbnailId && !i)) {
      thumbnailDivs[i].className = 'image-thumbnail-container-selected';
      selectedThumbnailId = thumbnailId;
      savedThumbnailIds[divId] = thumbnailId;
    } else {
      thumbnailDivs[i].className = 'image-thumbnail-container';
    }
  }
}

/**
 * Adds an image thumbnail to the specified div.
 */
function addScreenshot(divId, screenshot) {
  var thumbnailDiv = document.createElement('div');
  thumbnailDiv.className = 'image-thumbnail-container';

  thumbnailDiv.id = divId + '-thumbnailDiv-' + $(divId).children.length;
  thumbnailDiv.onclick = function() {
    selectImage(divId, thumbnailDiv.id);
  };

  var innerDiv = document.createElement('div');
  innerDiv.className = 'image-thumbnail';

  var thumbnail = document.createElement('img');
  thumbnail.id = thumbnailDiv.id + '-image';
  // We add the ?+timestamp to make sure the image URLs are unique
  // and Chrome does not load the image from cache.
  thumbnail.src = screenshot + "?" + Date.now();
  innerDiv.appendChild(thumbnail);

  var largeImage = document.createElement('img');
  largeImage.src = screenshot + "?" + Date.now();

  var popupDiv = document.createElement('div');
  popupDiv.appendChild(largeImage);
  innerDiv.appendChild(popupDiv);

  thumbnailDiv.appendChild(innerDiv);
  $(divId).appendChild(thumbnailDiv);

  if (!selectedThumbnailId)
    selectImage(divId, thumbnailDiv.id);
}

/**
 * Send's the report; after the report is sent, we need to be redirected to
 * the landing page, but we shouldn't be able to navigate back, hence
 * we open the landing page in a new tab and sendReport closes this tab.
 */
function sendReport() {
  if (!$('issue-with-combo').selectedIndex) {
    alert(localStrings.getString('no-issue-selected'));
    return false;
  }

  var imagePath = '';
  if (selectedThumbnailId)
    imagePath = $(selectedThumbnailId + '-image').src;

  // Note, categories are based from 1 in our protocol buffers, so no
  // adjustment is needed on selectedIndex.
  var reportArray = [String($('issue-with-combo').selectedIndex),
                     $('page-url-text').value,
                     $('description-text').value,
                     imagePath];

  // Add chromeos data if it exists.
  if ($('user-email-text') && $('sys-info-checkbox')) {
    reportArray = reportArray.concat([$('user-email-text').value,
                                      String($('sys-info-checkbox').checked)]);
  }

  // open the landing page in a new tab, sendReport will close this one.
  window.open(FEEDBACK_LANDING_PAGE, '_blank');
  chrome.send('sendReport', reportArray);
  return true;
}

function cancel() {
  chrome.send('cancel', []);
  return true;
}

/**
 * Select the current screenshots div, restoring the image that was
 * selected when we had this div open previously.
 */
function currentSelected() {
  $('current-screenshots').style.display = 'block';
  if ($('saved-screenshots'))
    $('saved-screenshots').style.display = 'none';

  if (selectedThumbnailDivId != 'current-screenshots')
    selectImage('current-screenshots',
                savedThumbnailIds['current-screenshots']);

  return true;
}

/**
 * Select the saved screenshots div, restoring the image that was
 * selected when we had this div open previously.
 */
function savedSelected() {
  $('current-screenshots').style.display = 'none';
  $('saved-screenshots').style.display = 'block';

  if (selectedThumbnailDivId != 'saved-screenshots')
    selectImage('saved-screenshots', savedThumbnailIds['saved-screenshots']);

  return true;
}

/**
 * Unselect all screenshots divs.
 */
function noneSelected() {
  $('current-screenshots').style.display = 'none';
  if ($('saved-screenshots'))
    $('saved-screenshots').style.display = 'none';

  selectedThumbnailDivId = '';
  selectedThumbnailId = '';
  return true;
}
