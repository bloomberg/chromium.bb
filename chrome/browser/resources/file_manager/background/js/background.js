// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Number of runtime errors catched in the background page.
 * @type {number}
 */
var JSErrorCount = 0;

/**
 * Map of all currently open app window. The key is an app id.
 * @type {Object.<string, AppWindow>}
 */
var appWindows = {};

/**
 * Synchronous queue for asynchronous calls.
 * @type {AsyncUtil.Queue}
 */
var queue = new AsyncUtil.Queue();

/**
 * Type of a Files.app's instance launch.
 * @enum {number}
 */
var LaunchType = Object.freeze({
  ALWAYS_CREATE: 0,
  FOCUS_ANY_OR_CREATE: 1,
  FOCUS_SAME_OR_CREATE: 2
});

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
 * @param {Object} options Options object to create it.
 * @constructor
 */
function AppWindowWrapper(url, id, options) {
  this.url_ = url;
  this.id_ = id;
  // Do deep copy for the template of options to assign own ID to the option
  // params.
  this.options_ = JSON.parse(JSON.stringify(options));
  this.options_.id = url; // This is to make Chrome reuse window geometries.
  this.window_ = null;
  this.appState_ = null;
  this.openingOrOpened_ = false;
  this.queue = new AsyncUtil.Queue();
  Object.seal(this);
}

/**
 * Shift distance to avoid overlapping windows.
 * @type {number}
 * @const
 */
AppWindowWrapper.SHIFT_DISTANCE = 40;

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
 * @param {function()=} opt_callback Completion callback.
 */
AppWindowWrapper.prototype.launch = function(appState, opt_callback) {
  // Check if the window is opened or not.
  if (this.openingOrOpened_) {
    console.error('The window is already opened.');
    if (opt_callback)
      opt_callback();
    return;
  }
  this.openingOrOpened_ = true;

  // Save application state.
  this.appState_ = appState;

  // Get similar windows, it means with the same initial url, eg. different
  // main windows of Files.app.
  var similarWindows = this.getSimilarWindows_();

  // Restore maximized windows, to avoid hiding them to tray, which can be
  // confusing for users.
  this.queue.run(function(nextStep) {
    for (var index = 0; index < similarWindows.length; index++) {
      if (similarWindows[index].isMaximized()) {
        var createWindowAndRemoveListener = function() {
          similarWindows[index].onRestored.removeListener(
              createWindowAndRemoveListener);
          nextStep();
        };
        similarWindows[index].onRestored.addListener(
            createWindowAndRemoveListener);
        similarWindows[index].restore();
        return;
      }
    }
    // If no maximized windows, then create the window immediately.
    nextStep();
  });

  // Closure creating the window, once all preprocessing tasks are finished.
  this.queue.run(function(nextStep) {
    chrome.app.window.create(this.url_, this.options_, function(appWindow) {
      this.window_ = appWindow;
      nextStep();
    }.bind(this));
  }.bind(this));

  // After creating.
  this.queue.run(function(nextStep) {
    var appWindow = this.window_;
    if (similarWindows.length) {
      // If we have already another window of the same kind, then shift this
      // window to avoid overlapping with the previous one.

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
      this.openingOrOpened_ = false;
      maybeCloseBackgroundPage();
    }.bind(this));

    if (opt_callback)
      opt_callback();

    nextStep();
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
 * @param {Object|function()} options Options object or a function to return it.
 * @constructor
 */
function SingletonAppWindowWrapper(url, options) {
  AppWindowWrapper.call(this, url, url, options);
}

/**
 * Inherits from AppWindowWrapper.
 */
SingletonAppWindowWrapper.prototype = {__proto__: AppWindowWrapper.prototype};

/**
 * Open the window.
 *
 * Activates an existing window or creates a new one.
 *
 * @param {Object} appState App state.
 * @param {function()=} opt_callback Completion callback.
 */
SingletonAppWindowWrapper.prototype.launch = function(appState, opt_callback) {
  // If the window is not opened yet, just call the parent method.
  if (!this.openingOrOpened_) {
    AppWindowWrapper.prototype.launch.call(this, appState, opt_callback);
    return;
  }

  // If the window is already opened, reload the window.
  // The queue is used to wait until the window is opened.
  this.queue.run(function(nextStep) {
    this.window_.contentWindow.appState = appState;
    this.window_.contentWindow.reload();
    this.window_.focus();
    if (opt_callback)
      opt_callback();
    nextStep();
  }.bind(this));
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
    this.launch(appState);
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
 * File manager window create options.
 * @type {Object}
 * @const
 */
var FILE_MANAGER_WINDOW_CREATE_OPTIONS = Object.freeze({
  defaultLeft: Math.round(window.screen.availWidth * 0.1),
  defaultTop: Math.round(window.screen.availHeight * 0.1),
  defaultWidth: Math.round(window.screen.availWidth * 0.8),
  defaultHeight: Math.round(window.screen.availHeight * 0.8),
  minWidth: 320,
  minHeight: 240,
  frame: 'none',
  hidden: true,
  transparentBackground: true,
  singleton: false
});

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
        'main.html',
        appId,
        FILE_MANAGER_WINDOW_CREATE_OPTIONS);
    appWindow.launch(opt_appState || {}, function() {
      if (opt_callback)
        opt_callback(appId);
      onTaskCompleted();
    });
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
  var urls = details.entries.map(function(e) { return e.toURL(); });

  switch (action) {
    case 'play':
      launchAudioPlayer({items: urls, position: 0});
      break;

    case 'watch':
      launchVideoPlayer(urls[0]);
      break;

    default:
      var launchEnable = null;
      var queue = new AsyncUtil.Queue();
      queue.run(function(nextStep) {
        // If it is not auto-open (triggered by mounting external devices), we
        // always launch Files.app.
        if (action != 'auto-open') {
          launchEnable = true;
          nextStep();
          return;
        }
        // If the disable-default-apps flag is on, Files.app is not opened
        // automatically on device mount because it obstculs the manual test.
        chrome.commandLinePrivate.hasSwitch('disable-default-apps',
                                            function(flag) {
          launchEnable = !flag;
          nextStep();
        });
      });
      queue.run(function(nextStep) {
        if (!launchEnable) {
          nextStep();
          return;
        }

        // Every other action opens a Files app window.
        var appState = {
          params: {
            action: action
          },
          defaultPath: details.entries[0].fullPath
        };
        // For mounted devices just focus any Files.app window. The mounted
        // volume will appear on the navigation list.
        var type = action == 'auto-open' ? LaunchType.FOCUS_ANY_OR_CREATE :
            LaunchType.FOCUS_SAME_OR_CREATE;
        launchFileManager(appState, /* App ID */ undefined, type, nextStep);
      });
      break;
  }
}

/**
 * Audio player window create options.
 * @type {Object}
 * @const
 */
var AUDIO_PLAYER_CREATE_OPTIONS = Object.freeze({
  type: 'panel',
  hidden: true,
  minHeight: 35 + 58,
  minWidth: 280,
  height: 35 + 58,
  width: 280,
  singleton: false
});

var audioPlayer = new SingletonAppWindowWrapper('mediaplayer.html',
                                                AUDIO_PLAYER_CREATE_OPTIONS);

/**
 * Launch the audio player.
 * @param {Object} playlist Playlist.
 */
function launchAudioPlayer(playlist) {
  audioPlayer.launch(playlist);
}

var videoPlayer = new SingletonAppWindowWrapper('video_player.html',
                                                {hidden: true});

/**
 * Launch the video player.
 * @param {string} url Video url.
 */
function launchVideoPlayer(url) {
  videoPlayer.launch({url: url});
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
  launchFileManager(null, null, LaunchType.FOCUS_ANY_OR_CREATE);
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
      !FileOperationManager.getInstance().hasQueuedTasks())
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
      loadTimeData.data = strings;
      initContextMenu(strings);
      chrome.storage.local.set({strings: strings}, callback);
    });
  });

  // Count runtime JavaScript errors.
  window.onerror = function() {
    JSErrorCount++;
  };
}

// Initialize Files.app.
initApp();

/**
 * Progress center of the background page.
 * @type {ProgressCenter}
 */
window.progressCenter = new ProgressCenter();

/**
 * Event handler for progress center.
 * @type {ProgressCenter}
 */
var progressCenterHandler = new ProgressCenterHandler(
    FileOperationManager.getInstance(),
    window.progressCenter);
