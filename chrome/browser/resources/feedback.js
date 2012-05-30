// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants.
/** @const */ var FEEDBACK_LANDING_PAGE =
    'https://www.google.com/support/chrome/go/feedback_confirmation';

var selectedThumbnailDivId = '';
var selectedThumbnailId = '';
var selectedImageUrl;

var savedThumbnailIds = [];
savedThumbnailIds['current-screenshots'] = '';
savedThumbnailIds['saved-screenshots'] = '';

var categoryTag = '';

/**
 * Selects an image thumbnail in the specified div.
 * @param {string} divId The id of the div to search in.
 * @param {string} thumbnailId The id of the thumbnail to search for.
 */
function selectImage(divId, thumbnailId) {
  var thumbnailDivs = $(divId).children;
  selectedThumbnailDivId = divId;
  if (thumbnailDivs.length == 0) {
    $(divId).hidden = true;
    return;
  }

  for (var i = 0; i < thumbnailDivs.length; i++) {
    thumbnailDivs[i].className = 'image-thumbnail-container';

    // If the the current div matches the thumbnail id provided,
    // or there is no thumbnail id given, and we're at the first thumbnail.
    if (thumbnailDivs[i].id == thumbnailId || (!thumbnailId && !i)) {
      thumbnailDivs[i].classList.add('image-thumbnail-container-selected');
      selectedThumbnailId = thumbnailId;
      savedThumbnailIds[divId] = thumbnailId;
    }
  }
}

/**
 * Adds an image thumbnail to the specified div.
 * @param {string} divId The id of the div to add a screenshot to.
 * @param {string} screenshot The URL of the screenshot being added.
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
  thumbnail.src = screenshot + '?' + Date.now();
  innerDiv.appendChild(thumbnail);

  thumbnailDiv.appendChild(innerDiv);
  $(divId).appendChild(thumbnailDiv);

  if (!selectedThumbnailId)
    selectImage(divId, thumbnailDiv.id);
}

/**
 * Disables screenshots completely.
 */
function disableScreenshots() {
  $('screenshot-row').hidden = true;
  $('screenshot-checkbox').checked = false;

  $('current-screenshots').hidden = true;
  if ($('saved-screenshots'))
    $('saved-screenshots').hidden = true;
}

/**
 * Sends the report; after the report is sent, we need to be redirected to
 * the landing page, but we shouldn't be able to navigate back, hence
 * we open the landing page in a new tab and sendReport closes this tab.
 * @return {boolean} True if the report was sent.
 */
function sendReport() {
  if ($('description-text').value.length == 0) {
    alert(loadTimeData.getString('no-description'));
    return false;
  }

  var imagePath = '';
  if ($('screenshot-checkbox').checked && selectedThumbnailId)
    imagePath = $(selectedThumbnailId + '-image').src;
  var pageUrl = $('page-url-text').value;
  if (!$('page-url-checkbox').checked)
    pageUrl = '';

  var reportArray = [pageUrl,
                     categoryTag,
                     $('description-text').value,
                     imagePath];

  // Add chromeos data if it exists.
  if ($('user-email-text') && $('sys-info-checkbox')) {
    var userEmail = $('user-email-text').textContent;
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

/**
 * Click listener for the cancel button.
 * @param {Event} e The click event being handled.
 */
function cancel(e) {
  chrome.send('cancel');
  e.preventDefault();
}

/**
 * Select the current screenshots div, restoring the image that was
 * selected when we had this div open previously.
 */
function currentSelected() {
  // TODO(rkc): Change this to use a class instead.
  $('current-screenshots').hidden = false;
  if ($('saved-screenshots'))
    $('saved-screenshots').hidden = true;

  if (selectedThumbnailDivId != 'current-screenshots')
    selectImage('current-screenshots',
                savedThumbnailIds['current-screenshots']);
}

/**
 * Select the saved screenshots div, restoring the image that was
 * selected when we had this div open previously.
 */
function savedSelected() {
  if ($('saved-screenshots').childElementCount == 0) {
    // setupSavedScreenshots will take care of changing visibility
    chrome.send('refreshSavedScreenshots');
  } else {
    $('current-screenshots').hidden = true;
    $('saved-screenshots').hidden = false;
    if (selectedThumbnailDivId != 'saved-screenshots')
      selectImage('saved-screenshots', savedThumbnailIds['saved-screenshots']);
  }
}

/**
 * Change the type of screenshot we're showing to the user from
 * the current screenshot to saved screenshots
 */
function changeToSaved() {
  $('screenshot-label-current').hidden = true;
  $('screenshot-label-saved').hidden = false;

  // Change the link to say "go to original"
  $('screenshot-link-tosaved').hidden = true;
  $('screenshot-link-tocurrent').hidden = false;

  savedSelected();
}

/**
 * Change the type of screenshot we're showing to the user from
 * the saved screenshots to the current screenshots
 */
function changeToCurrent() {
  $('screenshot-label-current').hidden = false;
  $('screenshot-label-saved').hidden = true;

  // Change the link to say "go to saved"
  $('screenshot-link-tosaved').hidden = false;
  $('screenshot-link-tocurrent').hidden = true;

  currentSelected();
}

///////////////////////////////////////////////////////////////////////////////
// Document Functions:
/**
 * Window onload handler, sets up the page.
 */
function load() {
  if ($('sysinfo-url')) {
    $('sysinfo-url').onclick = function(event) {
      chrome.send('openSystemTab');
    };
  }

<if expr="pp_ifdef('chromeos')">
  $('screenshot-link-tosaved').onclick = changeToSaved;
  $('screenshot-link-tocurrent').onclick = changeToCurrent;
</if>
  $('send-report-button').onclick = sendReport;
  $('cancel-button').onclick = cancel;

  // Set default values for the possible parameters, and then parse the actual
  // values from the URL hash.
  var parameters = {
    'description': '',
    'categoryTag': '',
    'customPageUrl': '',
  };
  var queryPos = window.location.hash.indexOf('?');
  if (queryPos !== -1) {
    // Get an array of parameters in 'name=value' form.
    var query = window.location.hash.substring(queryPos + 1).split('&');
    for (var i = 0; i < query.length; i++) {
      // Decode and store each parameter value.
      parameter = query[i].split('=');
      parameters[parameter[0]] = decodeURIComponent(parameter[1]);
    }

    // For a clean URL, trim the parameters from the hash.
    window.location.hash = window.location.hash.substring(0, queryPos);
  }

  // Set the initial description text.
  $('description-text').textContent = parameters['description'];
  // If a page url is spcified in the parameters, override the default page url.
  if (parameters['customPageUrl'] != '') {
    $('page-url-text').value = parameters['customPageUrl'];
    // and disable the page image, since it doesn't make sense on a custum url.
    disableScreenshots();
  }

  // Pick up the category tag (for most cases this will be an empty string)
  categoryTag = parameters['categoryTag'];

  chrome.send('getDialogDefaults');
  chrome.send('refreshCurrentScreenshot');
}

function setupCurrentScreenshot(screenshot) {
  addScreenshot('current-screenshots', screenshot);
}

function setupSavedScreenshots(screenshots) {
  if (screenshots.length == 0) {
    $('saved-screenshots').textContent =
        loadTimeData.getString('no-saved-screenshots');

    // Make sure we make the display the message.
    $('current-screenshots').hidden = true;
    $('saved-screenshots').hidden = false;

    // In case the user tries to send now; fail safe, do not send a screenshot
    // at all versus sending the current screenshot.
    selectedThumbnailDivId = '';
    selectedThumbnailId = '';
  } else {
    for (i = 0; i < screenshots.length; ++i)
      addScreenshot('saved-screenshots', screenshots[i]);

    // Now that we have our screenshots, try selecting the saved screenshots
    // again.
    savedSelected();
  }
}

function setupDialogDefaults(defaults) {
  if (defaults.length > 0) {
    if ($('page-url-text').value == '')
      $('page-url-text').value = defaults[0];
    if (defaults[0] == '')
      $('page-url-checkbox').checked = false;

    if (defaults.length > 2) {
      // We're in Chromium OS.
      $('user-email-text').textContent = defaults[2];
      if (defaults[2] == '') {
        // if we didn't get an e-mail address from cros,
        // disable the user email display totally.
        $('user-email-table').hidden = true;

        // this also means we are in privacy mode, so no saved screenshots.
        $('screenshot-link-tosaved').hidden = true;
      }
    }
  }
}

window.addEventListener('DOMContentLoaded', load);
