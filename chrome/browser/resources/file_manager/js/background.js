// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function launchFileManager(launchData) {
  var options = {
    defaultWidth: Math.round(window.screen.availWidth * 0.8),
    defaultHeight: Math.round(window.screen.availHeight * 0.8)
  };

  chrome.app.window.create('main.html', options, function(newWindow) {
    newWindow.contentWindow.launchData = launchData;
  });
}

chrome.app.runtime.onLaunched.addListener(launchFileManager);

chrome.fileBrowserHandler.onExecute.addListener(function(id, details) {
  var urls = details.entries.map(function(e) { return e.toURL() });
  switch (id) {
    case 'play':
      launchAudioPlayer({items: urls, position: 0});
      break;

    case 'watch':
      launchVideoPlayer(urls[0]);
      break;

    default:
      // Every other action opens the main Files app window.
      var launchData = {};
      launchData.action = id;
      launchData.defaultPath = details.entries[0].fullPath;
      launchFileManager(launchData);
      break;
  }
});

/**
 * Wrapper for a singleton app window.
 * @constructor
 */
function SingletonWindow() {
  this.window_ = null;
  this.creating_ = false;
}

/**
 * Open a singleton window.
 *
 * Expects the content window scripts to have |reload| and |unload| methods.
 *
 * @param {string} url Content page url.
 * @param {CreateWindowOptions} options Create open options.
 * @param {Object} launchData Launch data for the content window.
 */
SingletonWindow.prototype.open = function(url, options, launchData) {
  if (this.window_ && !this.window_.contentWindow.closed) {
    this.window_.focus();
    this.window_.contentWindow.reload(launchData);
    return;
  }

  this.launchData_ = launchData;
  if (this.creating_)
    return; // The window is being created, will use the updated launch data.

  options.id = url;
  this.creating_ = true;
  chrome.app.window.create(url, options, function(newWindow) {
    this.window_ = newWindow;
    this.window_.contentWindow.launchData = this.launchData_;
    this.creating_ = false;
    this.window_.onClosed.addListener(function() {
      this.window_.contentWindow_.unload();
    }.bind(this));
  }.bind(this));
};

var audioPlayer = new SingletonWindow();

/**
 * Launch the audio player or load the playlist into the existing player.
 *
 * @param {Object} playlist Playlist.
 */
function launchAudioPlayer(playlist) {
  var WIDTH = 280;
  var MIN_HEIGHT = 35 + 58;
  var MAX_HEIGHT = 35 + 58 * 3;
  var BOTTOM = 80;
  var RIGHT = 20;

  var options = {
    defaultLeft: (window.screen.availWidth - WIDTH - RIGHT),
    defaultTop: (window.screen.availHeight - MIN_HEIGHT - BOTTOM),
    minHeight: MIN_HEIGHT,
    maxHeight: MAX_HEIGHT,
    height: MIN_HEIGHT,
    minWidth: WIDTH,
    maxWidth: WIDTH,
    width: WIDTH
  };

  audioPlayer.open('mediaplayer.html', options, playlist);
}

var videoPlayer = new SingletonWindow();

/**
 * Launch video player.
 * @param {string} url Video url.
 */
function launchVideoPlayer(url) {
  var options = {
    hidden: true  // Will be shown when the video loads.
  };

  videoPlayer.open('video_player.html', options, {url: url});
}
