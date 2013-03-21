// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Map of all currently open app window. The key is an app id.
 */
var appWindows = {};

/**
 * @return {Array.<DOMWindow>} Array of content windows for all currently open
 *   app windows.
 */
function getContentWindows() {
  var views = [];
  for (var key in appWindows) {
    if (appWindows.hasOwnProperty(key))
      views.push(appWindows[key].contentWindow);
  }
  return views;
}

/**
 * Wrapper for an app window.
 *
 * Expects the following from the app scripts:
 * 1. The page load handler should initialize the app using |window.appState|
 *    and call |util.platform.saveAppState|.
 * 2. Every time the app state changes the app should update |window.appState|
 *    and call |util.platform.saveAppState| .
 * 3. The app may have |unload| function to persist the app state that does not
 *    fit into |window.appState|.
 *
 * @param {string} url App window content url.
 * @param {string} id App window id.
 * @param {Object|function} options Options object or a function to create it.
 * @constructor
 */
function AppWindowWrapper(url, id, options) {
  this.url_ = url;
  this.id_ = id;
  this.options_ = options;

  this.window_ = null;
  this.creating_ = false;
}

/**
 * @return {boolean} True if the window is currently open.
 */
AppWindowWrapper.prototype.isOpen = function() {
  return this.window_ && !this.window_.contentWindow.closed;
};

/**
 * Open the window.
 * @param {Object} appState App state.
 */
AppWindowWrapper.prototype.launch = function(appState) {
  console.assert(!this.isOpen(), 'The window is already open');
  console.assert(!this.creating_, 'The window is already opening');

  this.creating_ = true;
  this.appState_ = appState;

  var options = this.options_;
  if (typeof options == 'function')
    options = options();
  options.id = this.id_;

  chrome.app.window.create(this.url_, options, function(appWindow) {
    this.creating_ = false;
    this.window_ = appWindow;
    appWindows[this.id_] = appWindow;
    var contentWindow = appWindow.contentWindow;
    contentWindow.appID = this.id_;
    contentWindow.appState = this.appState_;
    appWindow.onClosed.addListener(function() {
      if (contentWindow.unload)
        contentWindow.unload();
      if (contentWindow.saveOnExit) {
        contentWindow.saveOnExit.forEach(function(entry) {
          util.AppCache.update(entry.key, entry.value);
        });
      }
      delete appWindows[this.id_];
      chrome.storage.local.remove(this.id_);  // Forget the persisted state.
      this.window_ = null;
    }.bind(this));
  }.bind(this));
};

/**
 * Wrapper for a singleton app window.
 *
 * In addition to the AppWindowWrapper requirements the app scripts should
 * have |reload| method that re-initializes the app based on a changed
 * |window.appState|.
 *
 * @param {string} url App window content url.
 * @param {Object|function} options Options object or a function to return it.
 * @constructor
 */
function SingletonAppWindowWrapper(url, options) {
  AppWindowWrapper.call(this, url, url, options);
}

/**
 * Inherits from AppWindowWrapper.
 */
SingletonAppWindowWrapper.prototype = { __proto__: AppWindowWrapper.prototype };

/**
 * Open the window.
 *
 * Activates an existing window or creates a new one.
 *
 * @param {Object} appState App state.
 */
SingletonAppWindowWrapper.prototype.launch = function(appState) {
  if (this.isOpen()) {
    this.window_.contentWindow.appState = appState;
    this.window_.contentWindow.reload();
    this.window_.focus();
    return;
  }

  if (this.creating_) {
    this.appState_ = appState;
    return;
  }

  AppWindowWrapper.prototype.launch.call(this, appState);
};

/**
 * Reopen a window if its state is saved in the local storage.
 */
SingletonAppWindowWrapper.prototype.reopen = function() {
  chrome.storage.local.get(this.id_, function(items) {
    var value = items[this.id_];
    if (!value)
      return;  // No app state persisted.

    try {
      var appState = JSON.parse(value);
      this.launch(appState);
    } catch (e) {
      console.error('Corrupt launch data for ' + this.id_, value);
    }
  }.bind(this));
};


/**
 * Prefix for the file manager window ID.
 */
var FILES_ID_PREFIX = 'files#';

/**
 * Regexp matching a file manager window ID.
 */
var FILES_ID_PATTERN = new RegExp('^' + FILES_ID_PREFIX + '(\\d*)$');

/**
 * Value of the next file manager window ID.
 */
var nextFileManagerWindowID = 0;

/**
 * @return {Object} File manager window create options.
 */
function createFileManagerOptions() {
  return {
    defaultWidth: Math.round(window.screen.availWidth * 0.8),
    defaultHeight: Math.round(window.screen.availHeight * 0.8)
  };
}

/**
 * @param {Object=} opt_appState App state.
 * @param {number=} opt_id Window id.
 */
function launchFileManager(opt_appState, opt_id) {
  var id = opt_id || nextFileManagerWindowID;
  nextFileManagerWindowID = Math.max(nextFileManagerWindowID, id + 1);

  var appWindow = new AppWindowWrapper(
        'main.html', FILES_ID_PREFIX + id, createFileManagerOptions);
  appWindow.launch(opt_appState || {});
}

/**
 * Relaunch file manager windows based on the persisted state.
 */
function reopenFileManagers() {
  chrome.storage.local.get(function(items) {
    for (var key in items) {
      if (items.hasOwnProperty(key)) {
        var match = key.match(FILES_ID_PATTERN);
        if (match) {
          var id = Number(match[1]);
          try {
            var appState = JSON.parse(items[key]);
            launchFileManager(appState, id);
          } catch (e) {
            console.error('Corrupt launch data for ' + id, value);
          }
        }
      }
    }
  });
}

/**
 * @param {string} action Task id.
 * @param {Object} details Details object.
 */
function executeFileBrowserTask(action, details) {
  var urls = details.entries.map(function(e) { return e.toURL() });
  switch (action) {
    case 'play':
      launchAudioPlayer({items: urls, position: 0});
      break;

    case 'watch':
      launchVideoPlayer(urls[0]);
      break;

    default:
      // Every other action opens a Files app window.
      launchFileManager({
        params: {
          action: action
        },
        defaultPath: details.entries[0].fullPath
      });
      break;
  }
}


/**
 * @return {Object} Audio player window create options.
 */
function createAudioPlayerOptions() {
  var WIDTH = 280;
  var MIN_HEIGHT = 35 + 58;
  var MAX_HEIGHT = 35 + 58 * 3;
  var BOTTOM = 80;
  var RIGHT = 20;

  return {
    defaultLeft: (window.screen.availWidth - WIDTH - RIGHT),
    defaultTop: (window.screen.availHeight - MIN_HEIGHT - BOTTOM),
    minHeight: MIN_HEIGHT,
    maxHeight: MAX_HEIGHT,
    height: MIN_HEIGHT,
    minWidth: WIDTH,
    maxWidth: WIDTH,
    width: WIDTH
  };
}

var audioPlayer =
    new SingletonAppWindowWrapper('mediaplayer.html', createAudioPlayerOptions);

/**
 * Launch the audio player.
 * @param {Object} playlist Playlist.
 */
function launchAudioPlayer(playlist) {
  audioPlayer.launch(playlist);
}

var videoPlayer =
    new SingletonAppWindowWrapper('video_player.html', { hidden: true });

/**
 * Launch the video player.
 * @param {string} url Video url.
 */
function launchVideoPlayer(url) {
  videoPlayer.launch({url: url});
}

/**
 * Launch the app.
 */
function launch() {
  if (nextFileManagerWindowID == 0) {
    // The app just launched. Remove window state records that are not needed
    // any more.
    chrome.storage.local.get(function(items) {
      for (var key in items) {
        if (items.hasOwnProperty(key)) {
          if (key.match(FILES_ID_PATTERN))
            chrome.storage.local.remove(key);
        }
      }
    });
  }

  launchFileManager();
}

/**
 * Restarted the app, restore windows.
 */
function restart() {
  reopenFileManagers();
  audioPlayer.reopen();
  videoPlayer.reopen();
}

chrome.app.runtime.onLaunched.addListener(launch);

chrome.app.runtime.onRestarted.addListener(restart);

function addExecuteHandler() {
  if (!chrome.fileBrowserHandler) {
    console.log('Running outside Chrome OS, loading mock API implementations.');
    util.loadScripts(['js/mock_chrome.js'], addExecuteHandler);
    return;
  }
  chrome.fileBrowserHandler.onExecute.addListener(executeFileBrowserTask);
}

addExecuteHandler();
