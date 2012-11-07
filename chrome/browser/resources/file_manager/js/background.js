// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('main.html', {
    height: 800,
    width: 1000
  });
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

  var param = {
    id: 'audio',
    defaultLeft: (window.screen.availWidth - WIDTH - RIGHT),
    defaultTop: (window.screen.availHeight - MIN_HEIGHT - BOTTOM),
    minHeight: MIN_HEIGHT,
    maxHeight: MAX_HEIGHT,
    height: MIN_HEIGHT,
    minWidth: WIDTH,
    maxWidth: WIDTH,
    width: WIDTH
  };

  audioPlayer.open('mediaplayer.html', param, playlist);
}

var videoPlayer = new SingletonWindow();

/**
 * Launch video player.
 * @param {string} url Video url.
 */
function launchVideoPlayer(url) {
  var param = {
    id: 'video',
    hidden: true  // Will be shown when the video loads.
  };

  videoPlayer.open('video_player.html', param, {url: url});
}
