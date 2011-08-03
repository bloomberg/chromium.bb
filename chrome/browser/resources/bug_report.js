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

  var menuOffPattern = /(^\?|&)menu=off($|&)/;
  var menuDisabled = menuOffPattern.test(window.location.search);
  document.documentElement.setAttribute('hide-menu', menuDisabled);

  // Set default values for the possible parameters, and then parse the actual
  // values from the URL hash.
  var parameters = {
    'description': '',
    'issueType': 0,
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

  // Get a list of issues that we allow the user to select from.
  // Note, the order and the issues types themselves are different
  // between Chromium and Chromium OS, so this code needs to be
  // maintained individually between in these two sections.
  var issueTypeText = [];
  issueTypeText[0] = localStrings.getString('issue-choose');
<if expr="not pp_ifdef('chromeos')">
  issueTypeText[1] = localStrings.getString('issue-page-formatting');
  issueTypeText[2] = localStrings.getString('issue-page-load');
  issueTypeText[3] = localStrings.getString('issue-plugins');
  issueTypeText[4] = localStrings.getString('issue-tabs');
  issueTypeText[5] = localStrings.getString('issue-sync');
  issueTypeText[6] = localStrings.getString('issue-crashes');
  issueTypeText[7] = localStrings.getString('issue-extensions');
  issueTypeText[8] = localStrings.getString('issue-phishing');
  issueTypeText[9] = localStrings.getString('issue-other');
  var numDefaultIssues = issueTypeText.length;
  issueTypeText[10] = localStrings.getString('issue-autofill');
</if>
<if expr="pp_ifdef('chromeos')">
  issueTypeText[1] = localStrings.getString('issue-connectivity');
  issueTypeText[2] = localStrings.getString('issue-sync');
  issueTypeText[3] = localStrings.getString('issue-crashes');
  issueTypeText[4] = localStrings.getString('issue-page-formatting');
  issueTypeText[5] = localStrings.getString('issue-extensions');
  issueTypeText[6] = localStrings.getString('issue-standby');
  issueTypeText[7] = localStrings.getString('issue-phishing');
  issueTypeText[8] = localStrings.getString('issue-other');
  var numDefaultIssues = issueTypeText.length;
  issueTypeText[9] = localStrings.getString('issue-autofill');
</if>
  // Add all the issues to the selection box.
  for (var i = 0; i < issueTypeText.length; i++) {
    var option = document.createElement('option');
    option.className = 'bug-report-text';
    option.textContent = issueTypeText[i];
    if (('' + i) === parameters['issueType'])
      option.selected = true;

    if (i < numDefaultIssues || option.selected)
      $('issue-with-combo').add(option);
  }

  chrome.send('getDialogDefaults', []);
  chrome.send('refreshCurrentScreenshot', []);
};

function setupCurrentScreenshot(screenshot) {
  addScreenshot('current-screenshots', screenshot);
}

function setupSavedScreenshots(screenshots) {
  if (screenshots.length == 0) {
    $('saved-screenshots').textContent =
        localStrings.getString('no-saved-screenshots');

    // Make sure we make the display the message.
    $('saved-screenshots').style.display = 'block';

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
    $('page-url-text').value = defaults[0];
    if (defaults[0] == '')
      $('page-url-checkbox').checked = false;

    if (defaults.length > 2) {
      // We're in Chromium OS.
      $('user-email-text').textContent = defaults[2];
      if (defaults[2] == '') {
        // if we didn't get an e-mail address from cros,
        // disable the user email display totally.
        $('user-email-table').style.display = 'none';

        // this also means we are in privacy mode, so no saved screenshots.
        $('screenshot-link-tosaved').style.display = 'none';
      }
    }
  }
}

window.addEventListener('DOMContentLoaded', load);
