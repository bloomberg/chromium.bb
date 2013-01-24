// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Display error message.
 * @param {string} opt_message Message id.
 */
function onError(opt_message) {
  var errorBanner = document.querySelector('#error');
  errorBanner.textContent =
      loadTimeData.getString(opt_message || 'GALLERY_VIDEO_ERROR');
  errorBanner.setAttribute('visible', 'true');
  if (util.platform.v2()) {
    // The window is hidden if the video has not loaded yet.
    chrome.app.window.current().show();
  }
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
      onError.bind(null, null),
      function() { chrome.fileBrowserPrivate.toggleFullscreen() },
      videoContainer);

  this.updateStyle();
  window.addEventListener('resize', this.updateStyle.bind(this));

  document.addEventListener('keydown', function(e) {
    if (e.keyIdentifier == 'U+0020') {
      this.togglePlayStateWithFeedback();
      e.preventDefault();
    }
  }.bind(this));

  videoContainer.addEventListener('click',
      this.togglePlayStateWithFeedback.bind(this));

  var inactivityWatcher = new MouseInactivityWatcher(playerContainer);
  inactivityWatcher.check();
}

FullWindowVideoControls.prototype = { __proto__: VideoControls.prototype };

/**
 * Save the current state so that it survives page/app reload.
 */
FullWindowVideoControls.prototype.onPlayStateChanged = function() {
  this.encodeState();
};

/**
 * Restore the state after the video is loaded.
 */
FullWindowVideoControls.prototype.restorePlayState = function() {
  if (!this.decodeState()) {
    VideoControls.prototype.restorePlayState.apply(this, arguments);
    this.play();
  }
};

// TODO(mtomasz): Convert it to class members: crbug.com/171191.
var controls;
var metadataCache;
var volumeManager;
var selectedItemFilesystemPath;

/**
 * Initialize the video player window.
 */
function loadVideoPlayer() {
  document.ondragstart = function(e) { e.preventDefault() };

  chrome.fileBrowserPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;

    controls = new FullWindowVideoControls(
       document.querySelector('#video-player'),
       document.querySelector('#video-container'),
       document.querySelector('#controls'));

    var video = document.querySelector('video');
    controls.attachMedia(video);

    if (!util.platform.v2())
      window.addEventListener('unload', unload);

    metadataCache = MetadataCache.createFull();
    volumeManager = VolumeManager.getInstance();

    // If the video player is starting before the first instance of the File
    // Manager then it does not have access to filesystem URLs. Request it now.
    chrome.fileBrowserPrivate.requestLocalFileSystem(reload);

    volumeManager.addEventListener('externally-unmounted',
                                   onExternallyUnmounted);
  });
}

/**
 * Closes video player when a volume containing the played item is unmounted.
 * @param {Event} event The unmount event.
 */
function onExternallyUnmounted(event) {
  if (!selectedItemFilesystemPath)
    return;
  if (selectedItemFilesystemPath.indexOf(event.mountPath) == 0)
    util.platform.closeWindow();
}

/**
 * Unload the player.
 */
function unload() {
  controls.savePosition(true /* exiting */);
  controls.cleanup();
}

/**
 * Reload the player.
 */
function reload() {
  var src;
  if (window.appState) {
    util.saveAppState();
    src = window.appState.url;
  } else {
    src = document.location.search.substr(1);
  }
  if (!src) {
    onError();
    return;
  }

  document.title = decodeURIComponent(src.split('/').pop());

  metadataCache.get(src, 'streaming', function(streaming) {
    if (streaming && streaming.url) {
      if (!navigator.onLine) {
        onError('GALLERY_VIDEO_OFFLINE');
        return;
      }
      src = streaming.url;
      console.log('Streaming: ' + src);
    } else {
      console.log('Playing local file: ' + src);
    }

    var video = document.querySelector('video');
    video.src = src;
    video.load();
    if (util.platform.v2()) {
      video.addEventListener('loadedmetadata', function() {
        // TODO: chrome.app.window soon will be able to resize the content area.
        // Until then use approximate title bar height.
        var TITLE_HEIGHT = 28;

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
      });
    }

    // Resolve real filesystem path of the current video.
    selectedItemFilesystemPath = null;
    webkitResolveLocalFileSystemURL(src,
      function(entry) {
        var video = document.querySelector('video');
        if (video.src != src) return;
        selectedItemFilesystemPath = entry.fullPath;
      });
  });
}

util.addPageLoadHandler(loadVideoPlayer);
