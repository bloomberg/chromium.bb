// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Displays error message.
 * @param {string} message Message id.
 */
function showErrorMessage(message) {
  var errorBanner = document.querySelector('#error');
  errorBanner.textContent =
      loadTimeData.getString(message);
  errorBanner.setAttribute('visible', 'true');

  // The window is hidden if the video has not loaded yet.
  chrome.app.window.current().show();
}

/**
 * Handles playback (decoder) errors.
 */
function onPlaybackError() {
  showErrorMessage('GALLERY_VIDEO_DECODING_ERROR');
  decodeErrorOccured = true;

  // Disable inactivity watcher, and disable the ui, by hiding tools manually.
  controls.inactivityWatcher.disabled = true;
  document.querySelector('#video-player').setAttribute('disabled', 'true');

  // Detach the video element, since it may be unreliable and reset stored
  // current playback time.
  controls.cleanup();
  controls.clearState();

  // Avoid reusing a video element.
  video.parentNode.removeChild(video);
  video = null;
}

/**
 * @param {Element} playerContainer Main container.
 * @param {Element} videoContainer Container for the video element.
 * @param {Element} controlsContainer Container for video controls.
 * @constructor
 */
function FullWindowVideoControls(
    playerContainer, videoContainer, controlsContainer) {
  VideoControls.call(this,
      controlsContainer,
      onPlaybackError,
      loadTimeData.getString.wrap(loadTimeData),
      this.toggleFullScreen_.wrap(this),
      videoContainer);

  this.playerContainer_ = playerContainer;

  this.updateStyle();
  window.addEventListener('resize', this.updateStyle.wrap(this));

  document.addEventListener('keydown', function(e) {
    if (e.keyIdentifier == 'U+0020') {  // Space
      this.togglePlayStateWithFeedback();
      e.preventDefault();
    }
    if (e.keyIdentifier == 'U+001B') {  // Escape
      util.toggleFullScreen(
          chrome.app.window.current(),
          false);  // Leave the full screen mode.
      e.preventDefault();
    }
  }.wrap(this));

  // TODO(mtomasz): Simplify. crbug.com/254318.
  videoContainer.addEventListener('click', function(e) {
    if (e.ctrlKey) {
      this.toggleLoopedModeWithFeedback(true);
      if (!this.isPlaying())
        this.togglePlayStateWithFeedback();
    } else {
      this.togglePlayStateWithFeedback();
    }
  }.wrap(this));

  this.inactivityWatcher_ = new MouseInactivityWatcher(playerContainer);
  this.__defineGetter__('inactivityWatcher', function() {
    return this.inactivityWatcher_;
  });

  this.inactivityWatcher_.check();
}

FullWindowVideoControls.prototype = { __proto__: VideoControls.prototype };

/**
 * Toggles the full screen mode.
 * @private
 */
FullWindowVideoControls.prototype.toggleFullScreen_ = function() {
  var appWindow = chrome.app.window.current();
  util.toggleFullScreen(appWindow, !util.isFullScreen(appWindow));
};

// TODO(mtomasz): Convert it to class members: crbug.com/171191.
var decodeErrorOccured;
var video;
var controls;

/**
 * Initializes the video player window.
 */
function loadVideoPlayer() {
  document.ondragstart = function(e) { e.preventDefault() };

  chrome.fileBrowserPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;

    var url = window.videoUrl;
    document.title = window.videoTitle;

    controls = new FullWindowVideoControls(
        document.querySelector('#video-player'),
        document.querySelector('#video-container'),
        document.querySelector('#controls'));

    var reloadVideo = function(e) {
      if (decodeErrorOccured &&
          // Ignore shortcut keys
          !e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey) {
        loadVideo(url);
        e.preventDefault();
      }
    };

    loadVideo(url);
    document.addEventListener('keydown', reloadVideo, true);
    document.addEventListener('click', reloadVideo, true);
  });
}

/**
 * Unloads the player.
 */
function unload() {
  if (!controls.getMedia())
    return;

  controls.savePosition(true /* exiting */);
  controls.cleanup();
}

/**
 * Reloads the player.
 * @param {string} url URL of the video file.
 */
function loadVideo(url) {
  // Re-enable ui and hide error message if already displayed.
  document.querySelector('#video-player').removeAttribute('disabled');
  document.querySelector('#error').removeAttribute('visible');
  controls.inactivityWatcher.disabled = false;
  decodeErrorOccured = false;

  // Detach the previous video element, if exists.
  if (video)
    video.parentNode.removeChild(video);

  video = document.createElement('video');
  document.querySelector('#video-container').appendChild(video);
  controls.attachMedia(video);

  video.src = url;
  video.load();
  video.addEventListener('loadedmetadata', function() {
    // TODO: chrome.app.window soon will be able to resize the content area.
    // Until then use approximate title bar height.
    var TITLE_HEIGHT = 33;

    var aspect = video.videoWidth / video.videoHeight;
    var newWidth = video.videoWidth;
    var newHeight = video.videoHeight + TITLE_HEIGHT;

    var shrinkX = newWidth / window.screen.availWidth;
    var shrinkY = newHeight / window.screen.availHeight;
    if (shrinkX > 1 || shrinkY > 1) {
      if (shrinkY > shrinkX) {
        newHeight = newHeight / shrinkY;
        newWidth = (newHeight - TITLE_HEIGHT) * aspect;
      } else {
        newWidth = newWidth / shrinkX;
        newHeight = newWidth / aspect + TITLE_HEIGHT;
      }
    }

    var oldLeft = window.screenX;
    var oldTop = window.screenY;
    var oldWidth = window.outerWidth;
    var oldHeight = window.outerHeight;

    if (!oldWidth && !oldHeight) {
      oldLeft = window.screen.availWidth / 2;
      oldTop = window.screen.availHeight / 2;
    }

    var appWindow = chrome.app.window.current();
    appWindow.resizeTo(newWidth, newHeight);
    appWindow.moveTo(oldLeft - (newWidth - oldWidth) / 2,
                     oldTop - (newHeight - oldHeight) / 2);
    appWindow.show();

    video.play();
  });
}

util.addPageLoadHandler(loadVideoPlayer);
