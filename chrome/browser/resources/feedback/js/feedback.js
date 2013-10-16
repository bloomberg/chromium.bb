// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {string}
 * @const
 */
var FEEDBACK_LANDING_PAGE =
    'https://www.google.com/support/chrome/go/feedback_confirmation';
/** @type {number}
 * @const
 */
var MAX_ATTACH_FILE_SIZE = 3 * 1024 * 1024;

var attachedFileBlob = null;
var lastReader = null;

var feedbackInfo = null;
var systemInfo = null;

var systemInfoWindowId = 0;

/**
 * Reads the selected file when the user selects a file.
 * @param {Event} fileSelectedEvent The onChanged event for the file input box.
 */
function onFileSelected(fileSelectedEvent) {
  $('attach-error').hidden = true;
  var file = fileSelectedEvent.target.files[0];
  if (!file) {
    // User canceled file selection.
    attachedFileBlob = null;
    return;
  }

  if (file.size > MAX_ATTACH_FILE_SIZE) {
    $('attach-error').hidden = false;

    // Clear our selected file.
    $('attach-file').value = '';
    attachedFileBlob = null;
    return;
  }

  attachedFileBlob = file.slice();
}

/**
 * Clears the file that was attached to the report with the initial request.
 * Instead we will now show the attach file button in case the user wants to
 * attach another file.
 */
function clearAttachedFile() {
  $('custom-file-container').hidden = true;
  attachedFileBlob = null;
  feedbackInfo.attachedFile = null;
  $('attach-file').hidden = false;
}

/**
 * Opens a new window with chrome://system, showing the current system info.
 */
function openSystemInfoWindow() {
  if (systemInfoWindowId == 0) {
    chrome.windows.create({url: 'chrome://system'}, function(win) {
      systemInfoWindowId = win.id;
      chrome.app.window.current().show();
    });
  } else {
    chrome.windows.update(systemInfoWindowId, {drawAttention: true});
  }
}

/**
 * Opens a new window with chrome://slow_trace, downloading performance data.
 */
function openSlowTraceWindow() {
  chrome.windows.create(
      {url: 'chrome://slow_trace/tracing.zip#' + feedbackInfo.traceId},
      function(win) {});
}

/**
 * Sends the report; after the report is sent, we need to be redirected to
 * the landing page, but we shouldn't be able to navigate back, hence
 * we open the landing page in a new tab and sendReport closes this tab.
 * @return {boolean} True if the report was sent.
 */
function sendReport() {
  if ($('description-text').value.length == 0) {
    var description = $('description-text');
    description.placeholder = loadTimeData.getString('no-description');
    description.focus();
    return false;
  }

  console.log('Feedback: Sending report');
  if (!feedbackInfo.attachedFile && attachedFileBlob) {
    feedbackInfo.attachedFile = { name: $('attach-file').value,
                                  data: attachedFileBlob };
  }

  feedbackInfo.description = $('description-text').value;
  feedbackInfo.pageUrl = $('page-url-text').value;
  feedbackInfo.email = $('user-email-text').value;

  var useSystemInfo = false;
  // On ChromeOS, since we gather System info, check if the user has given his
  // permission for us to send system info.
<if expr="pp_ifdef('chromeos')">
  if ($('sys-info-checkbox') != null &&
      $('sys-info-checkbox').checked &&
      systemInfo != null) {
    useSystemInfo = true;
  }
  if ($('performance-info-checkbox') == null ||
      !($('performance-info-checkbox').checked)) {
    feedbackInfo.traceId = null;
  }
</if>

// On NonChromeOS, we don't have any system information gathered except the
// Chrome version and the OS version. Hence for Chrome, pass the system info
// through.
<if expr="not pp_ifdef('chromeos')">
  if (systemInfo != null)
    useSystemInfo = true;
</if>

  if (useSystemInfo) {
    if (feedbackInfo.systemInformation != null) {
      // Concatenate sysinfo if we had any initial system information
      // sent with the feedback request event.
      feedbackInfo.systemInformation =
          feedbackInfo.systemInformation.concat(systemInfo);
    } else {
      feedbackInfo.systemInformation = systemInfo;
    }
  }

  // If the user doesn't want to send the screenshot.
  if (!$('screenshot-checkbox').checked)
    feedbackInfo.screenshot = null;

  chrome.feedbackPrivate.sendFeedback(feedbackInfo, function(result) {
    window.open(FEEDBACK_LANDING_PAGE, '_blank');
    window.close();
  });

  return true;
}

/**
 * Click listener for the cancel button.
 * @param {Event} e The click event being handled.
 */
function cancel(e) {
  e.preventDefault();
  window.close();
}

/**
 * Converts a blob data URL to a blob object.
 * @param {string} url The data URL to convert.
 * @return {Blob} Blob object containing the data.
 */
function dataUrlToBlob(url) {
  var mimeString = url.split(',')[0].split(':')[1].split(';')[0];
  var data = atob(url.split(',')[1]);
  var dataArray = [];
  for (var i = 0; i < data.length; ++i)
    dataArray.push(data.charCodeAt(i));

  return new Blob([new Uint8Array(dataArray)], {type: mimeString});
}

<if expr="pp_ifdef('chromeos')">
/**
 * Update the page when performance feedback state is changed.
 */
function performanceFeedbackChanged() {
  if ($('performance-info-checkbox').checked) {
    $('attach-file').disabled = true;
    $('attach-file').checked = false;

    $('screenshot-checkbox').disabled = true;
    $('screenshot-checkbox').checked = false;

    $('sys-info-checkbox').disabled = true;
    $('sys-info-checkbox').checked = false;
  } else {
    $('attach-file').disabled = false;
    $('screenshot-checkbox').disabled = false;
    $('sys-info-checkbox').disabled = false;
  }
}
</if>

/**
 * Initializes our page.
 * Flow:
 * .) DOMContent Loaded        -> . Request feedbackInfo object
 *                                . Setup page event handlers
 * .) Feedback Object Received -> . take screenshot
 *                                . request email
 *                                . request System info
 *                                . request i18n strings
 * .) Screenshot taken         -> . Show Feedback window.
 */
function initialize() {
  // Add listener to receive the feedback info object.
  chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
    if (request.sentFromEventPage) {
      // TODO(rkc):  Remove logging once crbug.com/284662 is closed.
      console.log('FEEDBACK_DEBUG: Received feedbackInfo.');
      feedbackInfo = request.data;
      $('description-text').textContent = feedbackInfo.description;
      if (feedbackInfo.pageUrl)
        $('page-url-text').value = feedbackInfo.pageUrl;

      takeScreenshot(function(screenshotDataUrl) {
        $('screenshot-image').src = screenshotDataUrl;
        feedbackInfo.screenshot = dataUrlToBlob(screenshotDataUrl);
        // TODO(rkc):  Remove logging once crbug.com/284662 is closed.
        console.log('FEEDBACK_DEBUG: Taken screenshot. Showing window.');
        chrome.app.window.current().show();
      });

      chrome.feedbackPrivate.getUserEmail(function(email) {
        $('user-email-text').value = email;
      });

      chrome.feedbackPrivate.getSystemInformation(function(sysInfo) {
        systemInfo = sysInfo;
      });

      // An extension called us with an attached file.
      if (feedbackInfo.attachedFile) {
        $('attached-filename-text').textContent =
            feedbackInfo.attachedFile.name;
        attachedFileBlob = feedbackInfo.attachedFile.data;
        $('custom-file-container').hidden = false;
        $('attach-file').hidden = true;
      }

<if expr="pp_ifdef('chromeos')">
      if (feedbackInfo.traceId && ($('performance-info-area'))) {
        $('performance-info-area').hidden = false;
        $('performance-info-checkbox').checked = true;
        performanceFeedbackChanged();
        $('performance-info-link').onclick = openSlowTraceWindow;
      }
</if>

      chrome.feedbackPrivate.getStrings(function(strings) {
        loadTimeData.data = strings;
        i18nTemplate.process(document, loadTimeData);
      });
    }
  });

  window.addEventListener('DOMContentLoaded', function() {
    // Ready to receive the feedback object.
    chrome.runtime.sendMessage({ready: true});

    // Setup our event handlers.
    $('attach-file').addEventListener('change', onFileSelected);
    $('send-report-button').onclick = sendReport;
    $('cancel-button').onclick = cancel;
    $('remove-attached-file').onclick = clearAttachedFile;
<if expr="pp_ifdef('chromeos')">
    $('performance-info-checkbox').addEventListener(
        'change', performanceFeedbackChanged);
</if>

    chrome.windows.onRemoved.addListener(function(windowId, removeInfo) {
      if (windowId == systemInfoWindowId)
        systemInfoWindowId = 0;
    });
    if ($('sysinfo-url')) {
      $('sysinfo-url').onclick = openSystemInfoWindow;
    }
  });
}

initialize();
