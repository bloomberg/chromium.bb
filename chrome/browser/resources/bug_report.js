// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  if (divId == 'current-screenshots')
    innerDiv.className = 'image-thumbnail-current';
  else
    innerDiv.className = 'image-thumbnail';

  var thumbnail = document.createElement('img');
  thumbnail.id = thumbnailDiv.id + '-image';
  // We add the ?+timestamp to make sure the image URLs are unique
  // and Chrome does not load the image from cache.
  thumbnail.src = screenshot + '?' + Date.now();
  innerDiv.appendChild(thumbnail);

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
  } else if ($('description-text').value.length == 0) {
    alert(localStrings.getString('no-description'));
    return false;
  }

  var imagePath = '';
  if ($('screenshot-checkbox').checked && selectedThumbnailId)
    imagePath = $(selectedThumbnailId + '-image').src;
  var pageUrl = $('page-url-text').value;
  if (!$('page-url-checkbox').checked)
    pageUrl = '';

  // Note, categories are based from 1 in our protocol buffers, so no
  // adjustment is needed on selectedIndex.
  var reportArray = [String($('issue-with-combo').selectedIndex),
                     pageUrl,
                     $('description-text').value,
                     imagePath];

  // Add chromeos data if it exists.
  if ($('user-email-text') && $('sys-info-checkbox')) {
    var userEmail= $('user-email-text').textContent;
    if (!$('user-email-checkbox').checked)
      userEmail = '';
    reportArray = reportArray.concat([userEmail,
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
  // TODO(rkc): Change this to use a class instead.
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

  if ($('saved-screenshots').childElementCount == 0) {
    // setupSavedScreenshots will take care of changing visibility
    chrome.send('refreshSavedScreenshots', []);
  } else {
    $('saved-screenshots').style.display = 'block';
    if (selectedThumbnailDivId != 'saved-screenshots')
      selectImage('saved-screenshots', savedThumbnailIds['saved-screenshots']);
  }

  return true;
}


/**
 * Change the type of screenshot we're showing to the user from
 * the current screenshot to saved screenshots
 */
function changeToSaved() {
  $('screenshot-label-current').style.display = 'none';
  $('screenshot-label-saved').style.display = 'inline';

  // Change the link to say "go to original"
  $('screenshot-link-tosaved').style.display = 'none';
  $('screenshot-link-tocurrent').style.display = 'inline';

  savedSelected();
}

/**
 * Change the type of screenshot we're showing to the user from
 * the saved screenshots to the current screenshots
 */
function changeToCurrent() {
  $('screenshot-label-current').style.display = 'inline';
  $('screenshot-label-saved').style.display = 'none';

  // Change the link to say "go to original"
  $('screenshot-link-tosaved').style.display = 'inline';
  $('screenshot-link-tocurrent').style.display = 'none';

  currentSelected();
}
