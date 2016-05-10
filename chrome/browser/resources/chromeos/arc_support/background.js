// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * UI Pages. Note the order must be in sync with the
 * ArcAuthService::UIPage enum.
 * @type {Array<string>}
 */
var UI_PAGES = ['none',
                'start',
                'lso-loading',
                'lso',
                'arc-loading',
                'error'];

/**
 * Chrome window that hosts UI. Only one window is allowed.
 * @type {chrome.app.window.AppWindow}
 */
var appWindow = null;

/**
 * Contains Web content provided by Google authorization server.
 * @type {WebView}
 */
var webview = null;

/**
 * Used for bidirectional communication with native code.
 * @type {chrome.runtime.Port}
 */
var port = null;

/**
 * Indicates that window was closed internally and it is not required to send
 * closure notification.
 * @type {boolean}
 */
var windowClosedInternally = false;

/**
 * Closes current window in response to request from native code. This does not
 * issue 'cancelAuthCode' message to native code.
 */
function closeWindowInternally() {
  windowClosedInternally = true;
  appWindow.close();
  appWindow = null;
}

/**
 * Sends a native message to ArcSupportHost.
 * @param {string} code The action code in message.
 * @param {Object=} opt_Props Extra properties for the message.
 */
function sendNativeMessage(code, opt_Props) {
  var message = Object.assign({'action': code}, opt_Props);
  port.postMessage(message);
}

/**
 * Applies localization for html content.
 * @param {!Object} data Localized strings and relevant information.
 */
function setLocalization(data) {
  var doc = appWindow.contentWindow.document;
  var loadTimeData = appWindow.contentWindow.loadTimeData;
  loadTimeData.data = data;
  appWindow.contentWindow.i18nTemplate.process(doc, loadTimeData);
  doc.getElementById('legacy-text').innerHTML = data.greetingLegacy;
}

/**
 * Handles native messages received from ArcSupportHost.
 * @param {!Object} message The message received.
 */
function onNativeMessage(message) {
  if (!message.action) {
    return;
  }

  if (!appWindow) {
    console.warn('Received native message when window is not available.');
    return;
  }

  if (message.action == 'setLocalization') {
    setLocalization(message.data);
  } else if (message.action == 'closeUI') {
    closeWindowInternally();
  } else if (message.action == 'showPage') {
    showPageWithStatus(message.page, message.status);
  }
}

/**
 * Connects to ArcSupportHost.
 */
function connectPort() {
  var hostName = 'com.google.arc_support';
  port = chrome.runtime.connectNative(hostName);
  port.onMessage.addListener(onNativeMessage);
}

/**
 * Shows requested page and hide others. Show appWindow if it was hidden before
 * for non 'none' pages. For 'none' page this closes host window.
 * @param {string} pageDivId id of divider of the page to show.
 */
function showPage(pageDivId) {
  if (!appWindow) {
    return;
  }

  if (pageDivId == 'none') {
    closeWindowInternally();
    return;
  }

  var doc = appWindow.contentWindow.document;
  var pages = doc.getElementsByClassName('section');
  for (var i = 0; i < pages.length; i++) {
    pages[i].hidden = pages[i].id != pageDivId;
  }

  if (pageDivId == 'lso-loading') {
    webview.src = 'https://accounts.google.com/o/oauth2/v2/auth?client_id=' +
                  '1070009224336-sdh77n7uot3oc99ais00jmuft6sk2fg9.apps.' +
                  'googleusercontent.com&response_type=code&redirect_uri=oob&' +
                  'scope=https://www.google.com/accounts/OAuthLogin&' +
                  'device_type=arc_plus_plus&device_id=0';
  }
  appWindow.show();
}

/**
 * Sets error message.
 * @param {string} error message.
 */
function setErrorMessage(error) {
  if (!appWindow) {
    return;
  }
  var doc = appWindow.contentWindow.document;
  var messageElement = doc.getElementById('error-message');
  messageElement.innerText = error;
}

/**
 * Shows requested page.
 * @param {int} pageId Index of the page to show. Must be in the array range of
 * UI_PAGES.
 * @param {string} status associated with page string status, error message for
 *                        example.
 */
function showPageWithStatus(pageId, status) {
  if (!appWindow) {
    return;
  }

  if (UI_PAGES[pageId] == 'error') {
    setErrorMessage(status);
  }
  showPage(UI_PAGES[pageId]);
}

chrome.app.runtime.onLaunched.addListener(function() {
  var onAppContentLoad = function() {
    var doc = appWindow.contentWindow.document;
    webview = doc.getElementById('arc-support');
    // Apply absolute dimension to webview tag in order to avoid UI glitch
    // when embedded content layout is visible for user, even if 100% width and
    // height are set in css file.
    // TODO(khmel): Investigate why relative layout is not enough.
    webview.style.width = appWindow.innerBounds.width + 'px';
    webview.style.height = appWindow.innerBounds.height + 'px';

    var isApprovalResponse = function(url) {
      var resultUrlPrefix = 'https://accounts.google.com/o/oauth2/approval?';
      return url.substring(0, resultUrlPrefix.length) == resultUrlPrefix;
    };

    var onWebviewRequestResponseStarted = function(details) {
      if (isApprovalResponse(details.url)) {
        showPage('arc-loading');
      }
    };

    var onWebviewContentLoad = function() {
      if (!isApprovalResponse(webview.src)) {
        // Show LSO page when its content is ready.
        showPage('lso');
        return;
      }

      webview.executeScript({code: 'document.title;'}, function(results) {
        var authCodePrefix = 'Success code=';
        if (results[0].substring(0, authCodePrefix.length) ==
            authCodePrefix) {
          var authCode = results[0].substring(authCodePrefix.length);
          sendNativeMessage('setAuthCode', {code: authCode});
        } else {
          setErrorMessage(appWindow.contentWindow.loadTimeData.getString(
              'authorizationFailed'));
          showPage('error');
        }
      });
    };

    var requestFilter = {
      urls: ['<all_urls>'],
      types: ['main_frame']
    };

    webview.request.onResponseStarted.addListener(
        onWebviewRequestResponseStarted, requestFilter);
    webview.addEventListener('contentload', onWebviewContentLoad);

    var onGetStarted = function() {
      sendNativeMessage('startLso');
    };

    var onRetry = function() {
      sendNativeMessage('startLso');
    };

    doc.getElementById('get-started').addEventListener('click', onGetStarted);
    doc.getElementById('retry').addEventListener('click', onRetry);

    connectPort();
  };

  var onWindowCreated = function(createdWindow) {
    appWindow = createdWindow;
    appWindow.contentWindow.onload = onAppContentLoad;
    createdWindow.onClosed.addListener(onWindowClosed);
  };

  var onWindowClosed = function() {
    if (windowClosedInternally) {
      return;
    }
    sendNativeMessage('cancelAuthCode');
  };

  windowClosedInternally = false;
  var options = {
    'id': 'arc_support_wnd',
    'resizable': false,
    'hidden': true,
    'frame': {
      type: 'chrome',
      color: '#67a030'
    },
    'innerBounds': {
      'width': 800,
      'height': 600
    }
  };
  chrome.app.window.create('main.html', options, onWindowCreated);
});
