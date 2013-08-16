// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Map of all currently open app window. The key is an app id.
 */
var appWindows = {};

/**
 * Synchronous queue for asynchronous calls.
 * @type {AsyncUtil.Queue}
 */
var queue = new AsyncUtil.Queue();

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
 * Type of a Files.app's instance launch.
 * @enum {number}
 */
var LaunchType = {
  ALWAYS_CREATE: 0,
  FOCUS_ANY_OR_CREATE: 1,
  FOCUS_SAME_OR_CREATE: 2
};

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
 * @param {AsyncUtil.Queue} queue Queue for asynchronous window launches.
 * @param {string} url App window content url.
 * @param {string} id App window id.
 * @param {Object|function()} options Options object or a function to create it.
 * @constructor
 */
function AppWindowWrapper(queue, url, id, options) {
  this.queue_ = queue;
  this.url_ = url;
  this.id_ = id;
  this.options_ = options;
  this.window_ = null;
}

/**
 * Shift distance to avoid overlapping windows.
 * @type {number}
 * @const
 */
AppWindowWrapper.SHIFT_DISTANCE = 40;

/**
 * @return {boolean} True if the window is currently open.
 */
AppWindowWrapper.prototype.isOpen = function() {
  return this.window_ && !this.window_.contentWindow.closed;
};

/**
 * Gets similar windows, it means with the same initial url.
 * @return {Array.<AppWindow>} List of similar windows.
 * @private
 */
AppWindowWrapper.prototype.getSimilarWindows_ = function() {
  var result = [];
  for (var appID in appWindows) {
    if (appWindows[appID].contentWindow.appInitialURL == this.url_)
      result.push(appWindows[appID]);
  }
  return result;
};

/**
 * Opens the window.
 *
 * @param {Object} appState App state.
 * @param {function()} callback Completion callback.
 */
AppWindowWrapper.prototype.launch = function(appState, callback) {
  if (this.isOpen()) {
    console.error('The window is already open');
    callback();
    return;
  }
  this.appState_ = appState;

  var options = this.options_;
  if (typeof options == 'function')
    options = options();
  options.id = this.url_;  // This is to make Chrome reuse window geometries.
  options.singleton = false;

  // Get similar windows, it means with the same initial url, eg. different
  // main windows of Files.app.
  var similarWindows = this.getSimilarWindows_();

  // Closure creating the window, once all preprocessing tasks are finished.
  var createWindow = function() {
    chrome.app.window.onRestored.removeListener(createWindow);
    chrome.app.window.create(this.url_, options, function(appWindow) {
      this.window_ = appWindow;

      // If we have already another window of the same kind, then shift this
      // window to avoid overlapping with the previous one.
      if (similarWindows.length) {
        var bounds = appWindow.getBounds();
        appWindow.moveTo(bounds.left + AppWindowWrapper.SHIFT_DISTANCE,
                         bounds.top + AppWindowWrapper.SHIFT_DISTANCE);
      }

      appWindows[this.id_] = appWindow;
      var contentWindow = appWindow.contentWindow;
      contentWindow.appID = this.id_;
      contentWindow.appState = this.appState_;
      contentWindow.appInitialURL = this.url_;
      if (window.IN_TEST)
        contentWindow.IN_TEST = true;
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
        maybeCloseBackgroundPage();
      }.bind(this));

      callback();
    }.bind(this));
  }.bind(this);

  // Restore maximized windows, to avoid hiding them to tray, which can be
  // confusing for users.
  for (var index = 0; index < similarWindows.length; index++) {
    if (similarWindows[index].isMaximized()) {
      var createWindowAndRemoveListener = function() {
        createWindow();
        similarWindows[index].onRestored.removeListener(
            createWindowAndRemoveListener);
      };
      similarWindows[index].onRestored.addListener(
          createWindowAndRemoveListener);
      similarWindows[index].restore();
      return;
    }
  }

  // If no maximized windows, then create the window immediately.
  createWindow();
};

/**
 * Enqueues opening the window.
 * @param {Object} appState App state.
 */
AppWindowWrapper.prototype.enqueueLaunch = function(appState) {
  this.queue_.run(this.launch.bind(this, appState));
};

/**
 * Wrapper for a singleton app window.
 *
 * In addition to the AppWindowWrapper requirements the app scripts should
 * have |reload| method that re-initializes the app based on a changed
 * |window.appState|.
 *
 * @param {AsyncUtil.Queue} queue Queue for asynchronous window launches.
 * @param {string} url App window content url.
 * @param {Object|function()} options Options object or a function to return it.
 * @constructor
 */
function SingletonAppWindowWrapper(queue, url, options) {
  AppWindowWrapper.call(this, queue, url, url, options);
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
 * @param {function()} callback Completion callback.
 */
SingletonAppWindowWrapper.prototype.launch = function(appState, callback) {
  if (this.isOpen()) {
    this.window_.contentWindow.appState = appState;
    this.window_.contentWindow.reload();
    this.window_.focus();
    callback();
    return;
  }

  AppWindowWrapper.prototype.launch.call(this, appState, callback);
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
    } catch (e) {
      console.error('Corrupt launch data for ' + this.id_, value);
      return;
    }
    this.enqueueLaunch(appState);
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
    defaultLeft: Math.round(window.screen.availWidth * 0.1),
    defaultTop: Math.round(window.screen.availHeight * 0.1),
    defaultWidth: Math.round(window.screen.availWidth * 0.8),
    defaultHeight: Math.round(window.screen.availHeight * 0.8),
    minWidth: 320,
    minHeight: 240,
    frame: 'none',
    hidden: true,
    transparentBackground: true
  };
}

/**
 * @param {Object=} opt_appState App state.
 * @param {number=} opt_id Window id.
 * @param {LaunchType=} opt_type Launch type. Default: ALWAYS_CREATE.
 * @param {function(string)=} opt_callback Completion callback with the App ID.
 */
function launchFileManager(opt_appState, opt_id, opt_type, opt_callback) {
  var type = opt_type || LaunchType.ALWAYS_CREATE;

  // Wait until all windows are created.
  queue.run(function(onTaskCompleted) {
    // Check if there is already a window with the same path. If so, then
    // reuse it instead of opening a new one.
    if (type == LaunchType.FOCUS_SAME_OR_CREATE ||
        type == LaunchType.FOCUS_ANY_OR_CREATE) {
      if (opt_appState && opt_appState.defaultPath) {
        for (var key in appWindows) {
          var contentWindow = appWindows[key].contentWindow;
          if (contentWindow.appState &&
              opt_appState.defaultPath == contentWindow.appState.defaultPath) {
            appWindows[key].focus();
            if (opt_callback)
              opt_callback(key);
            onTaskCompleted();
            return;
          }
        }
      }
    }

    // Focus any window if none is focused. Try restored first.
    if (type == LaunchType.FOCUS_ANY_OR_CREATE) {
      // If there is already a focused window, then finish.
      for (var key in appWindows) {
        // The isFocused() method should always be available, but in case
        // Files.app's failed on some error, wrap it with try catch.
        try {
          if (appWindows[key].contentWindow.isFocused()) {
            if (opt_callback)
              opt_callback(key);
            onTaskCompleted();
            return;
          }
        } catch (e) {
          console.error(e.message);
        }
      }
      // Try to focus the first non-minimized window.
      for (var key in appWindows) {
        if (!appWindows[key].isMinimized()) {
          appWindows[key].focus();
          if (opt_callback)
            opt_callback(key);
          onTaskCompleted();
          return;
        }
      }
      // Restore and focus any window.
      for (var key in appWindows) {
        appWindows[key].focus();
        if (opt_callback)
          opt_callback(key);
        onTaskCompleted();
        return;
      }
    }

    // Create a new instance in case of ALWAYS_CREATE type, or as a fallback
    // for other types.

    var id = opt_id || nextFileManagerWindowID;
    nextFileManagerWindowID = Math.max(nextFileManagerWindowID, id + 1);
    var appId = FILES_ID_PREFIX + id;

    var appWindow = new AppWindowWrapper(
        queue,
        'main.html',
        appId,
        createFileManagerOptions);
    appWindow.enqueueLaunch(opt_appState || {});
    if (opt_callback)
      opt_callback(appId);
    onTaskCompleted();
  });
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
            console.error('Corrupt launch data for ' + id);
          }
        }
      }
    }
  });
}

/**
 * Executes a file browser task.
 *
 * @param {string} action Task id.
 * @param {Object} details Details object.
 */
function onExecute(action, details) {
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
      var appState = {
        params: {
          action: action
        },
        defaultPath: details.entries[0].fullPath,
      };
      // For mounted devices just focus any Files.app window. The mounted
      // volume will appear on the navigation list.
      var type = action == 'auto-open' ? LaunchType.FOCUS_ANY_OR_CREATE :
          LaunchType.FOCUS_SAME_OR_CREATE;
      launchFileManager(appState,
                        undefined,  // App ID.
                        type);
      break;
  }
}


/**
 * @return {Object} Audio player window create options.
 */
function createAudioPlayerOptions() {
  var WIDTH = 280;
  var HEIGHT = 35 + 58;
  return {
    type: 'panel',
    hidden: true,
    minHeight: HEIGHT,
    minWidth: WIDTH,
    height: HEIGHT,
    width: WIDTH
  };
}

var audioPlayer = new SingletonAppWindowWrapper(queue,
                                                'mediaplayer.html',
                                                createAudioPlayerOptions);

/**
 * Launch the audio player.
 * @param {Object} playlist Playlist.
 */
function launchAudioPlayer(playlist) {
  audioPlayer.enqueueLaunch(playlist);
}

var videoPlayer = new SingletonAppWindowWrapper(queue,
                                                'video_player.html',
                                                {hidden: true});

/**
 * Launch the video player.
 * @param {string} url Video url.
 */
function launchVideoPlayer(url) {
  videoPlayer.enqueueLaunch({url: url});
}

/**
 * Launches the app.
 */
function onLaunched() {
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
function onRestarted() {
  reopenFileManagers();
  audioPlayer.reopen();
  videoPlayer.reopen();
}

/**
 * Handles clicks on a custom item on the launcher context menu.
 * @param {OnClickData} info Event details.
 */
function onContextMenuClicked(info) {
  if (info.menuItemId == 'new-window') {
    // Find the focused window (if any) and use it's current path for the
    // new window. If not found, then launch with the default path.
    for (var key in appWindows) {
      try {
        if (appWindows[key].contentWindow.isFocused()) {
          var appState = {
            defaultPath: appWindows[key].contentWindow.appState.defaultPath
          };
          launchFileManager(appState);
          return;
        }
      } catch (ignore) {
        // The isFocused method may not be defined during initialization.
        // Therefore, wrapped with a try-catch block.
      }
    }

    // Launch with the default path.
    launchFileManager();
  }
}

/**
 * Closes the background page, if it is not needed.
 */
function maybeCloseBackgroundPage() {
  if (Object.keys(appWindows).length === 0 &&
      !FileCopyManager.getInstance().hasQueuedTasks())
    close();
}

/**
 * Initializes the context menu. Recreates if already exists.
 * @param {Object} strings Hash array of strings.
 */
function initContextMenu(strings) {
  try {
    chrome.contextMenus.remove('new-window');
  } catch (ignore) {
    // There is no way to detect if the context menu is already added, therefore
    // try to recreate it every time.
  }
  chrome.contextMenus.create({
    id: 'new-window',
    contexts: ['launcher'],
    title: strings['NEW_WINDOW_BUTTON_LABEL']
  });
}

/**
 * Initializes the background page of Files.app.
 */
function initApp() {
  // Initialize handlers.
  chrome.fileBrowserHandler.onExecute.addListener(onExecute);
  chrome.app.runtime.onLaunched.addListener(onLaunched);
  chrome.app.runtime.onRestarted.addListener(onRestarted);
  chrome.contextMenus.onClicked.addListener(onContextMenuClicked);

  // Fetch strings and initialize the context menu.
  queue.run(function(callback) {
    chrome.fileBrowserPrivate.getStrings(function(strings) {
      initContextMenu(strings);
      chrome.storage.local.set({strings: strings}, callback);
    });
  });
}

// Initialize Files.app.
initApp();
