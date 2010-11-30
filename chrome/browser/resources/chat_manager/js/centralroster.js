// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var MIN_RETRY_MILLISECONDS = 1 * 1000;
var MAX_RETRY_MILLISECONDS = 4 * 60 * 1000;
var retryTime;
var retryTimer;
var chatClient = null;

document.title = chrome.i18n.getMessage('CHAT_MANAGER_NAME');
var args = {
  'protocol': 'https',
  'host': 'talkgadget.google.com',
  'jsmode': 'pre',
  'hl': chrome.i18n.getMessage('@@ui_locale')
};

// Read args.
var urlParts = window.location.href.split(/[?&#]/);
for (var i = 1; i < urlParts.length; i++) {
  var argParts = urlParts[i].split('=');
  if (argParts.length == 2) {
    args[argParts[0]] = argParts[1];
  }
}
var notifierScriptUrl =
    args['protocol'] + '://' + args['host'] +
    '/talkgadget/notifier-js?silent=true&host=' +
    args['protocol'] + '://' + args['host'] +
    '/talkgadget/notifier-js' +
    (args['jsmode'] != '' ? ('&jsmode=' + args['jsmode']) : '');

// Implement Singleton pattern, where there is only one central roster.
function makeSingleton() {
  // This will keep a list of all central roster panels.
  var chat_popups = [];

  chrome.windows.getAll({populate: true}, function(wins) {
    // Find and remember all central roster panels.
    for (var winIdx = 0, win = wins[winIdx]; win; win = wins[++winIdx]) {
      var tabs = win.tabs;
      for (var tabIdx = 0, tab = tabs[tabIdx]; tab; tab = tabs[++tabIdx]) {
        if (tab.url === location.href) {
          chat_popups.push({tabId: tab.id, winId: win.id});
        }
      }
    }
    if (chat_popups.length > 0) {
      // Multiple panels are executing this function at the same time so we need
      // a global criteria to pickup a "winner": "keep only the tab with
      // smallest id".
      chat_popups.sort(function(t1, t2) { return t1.tabId - t2.tabId; });
      for (var i = 0, l = chat_popups.length; i < l; i++) {
        if (i == 0) {
          chrome.windows.update(chat_popups[i].winId, {focused: true});
        } else {
          chrome.tabs.remove(chat_popups[i].tabId);
        }
      }
    }
  });
}
makeSingleton();

function runGTalkScript() {
  var script = document.createElement('script');
  script.src = notifierScriptUrl;
  script.onload = loadGTalk;
  script.onerror = loadGTalk;
  document.body.appendChild(script);
}

function retryConnection() {
  location.reload();
}

function retryConnectionCountdown() {
  var seconds = retryTime / 1000;
  var minutes = Math.floor(seconds / 60);
  seconds -= minutes * 60;

  document.getElementById('retryStatus').textContent =
      chrome.i18n.getMessage('CHAT_MANAGER_RETRYING_IN',
          [minutes, (seconds < 10 ? '0' : '') + seconds]);

  if (retryTime <= 0) {
    retryConnection();
  } else {
    retryTimer = setTimeout(retryConnectionCountdown, 1000);
    retryTime -= 1000;
  }
}

function loadGTalk() {
  if (window.GTalkNotifier) {
    document.getElementById('retryInfo').style.display = 'none';
    var baseUrl = args['protocol'] + '://' + args['host'] + '/talkgadget/';
    chatClient = new window.GTalkNotifier(
        {
          'clientBaseUrl': baseUrl,
          'clientUrl': 'notifierclient' +
              (args['jsmode'] != '' ? ('?jsmode=' + args['jsmode']) : ''),
          'propertyName': 'ChromeOS',
          'xpcRelay': baseUrl + 'xpc_relay',
          'xpcBlank': baseUrl + 'xpc_blank',
          'locale': args['hl'],
          'isCentralRoster': true,
          'hideProfileCard': true,
          'isFullFrame': true
        }
    );
    delete localStorage.retryStartTime;
  } else {
    if (!localStorage.retryStartTime) {
      localStorage.retryStartTime = MIN_RETRY_MILLISECONDS;
    } else if (localStorage.retryStartTime < MAX_RETRY_MILLISECONDS) {
      localStorage.retryStartTime = Math.min(localStorage.retryStartTime * 2,
          MAX_RETRY_MILLISECONDS);
    }
    retryTime = localStorage.retryStartTime;
    document.getElementById('retryInfo').style.display = 'inline';
    retryConnectionCountdown();
  }
}

function onPageLoaded() {
  // Localizing the page content.
  document.getElementById('retryMessage').textContent =
      chrome.i18n.getMessage('CHAT_MANAGER_COULD_NOT_CONNECT');
  document.getElementById('retryButton').value =
      chrome.i18n.getMessage('CHAT_MANAGER_RETRY_NOW');

  if (localStorage.hasOpenedInViewer) {
    runGTalkScript();
  }
}
