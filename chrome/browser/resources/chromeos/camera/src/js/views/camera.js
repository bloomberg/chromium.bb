// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for views.
 */
camera.views = camera.views || {};

/**
 * Creates the Camera view controller.
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @implements {camera.models.Gallery.Observer}
 * @constructor
 */
camera.views.Camera = function(context, router) {
  camera.View.call(
      this, context, router, document.querySelector('#camera'), 'camera');

  /**
   * Gallery model used to save taken pictures.
   * @type {camera.models.Gallery}
   * @private
   */
  this.model_ = null;

  /**
   * Video element to catch the stream and plot it later onto a canvas.
   * @type {Video}
   * @private
   */
  this.video_ = document.createElement('video');

  /**
   * Current camera stream.
   * @type {MediaStream}
   * @private
   */
  this.stream_ = null;

  /**
   * MediaRecorder object to record motion pictures.
   * @type {MediaRecorder}
   * @private
   */
  this.mediaRecorder_ = null;

  /**
   * Last frame time, used to detect new frames of <video>.
   * @type {number}
   * @private
   */
  this.lastFrameTime_ = -1;

  /**
   * @type {?number}
   * @private
   */
  this.retryStartTimer_ = null;

  /**
   * @type {?number}
   * @private
   */
  this.watchdog_ = null;

  /**
   * Shutter sound player.
   * @type {Audio}
   * @private
   */
  this.shutterSound_ = document.createElement('audio');

  /**
   * Tick sound player.
   * @type {Audio}
   * @private
   */
  this.tickSound_ = document.createElement('audio');

  /**
   * Record-start sound player.
   * @type {Audio}
   * @private
   */
  this.recordStartSound_ = document.createElement('audio');

  /**
   * Record-end sound player.
   * @type {Audio}
   * @private
   */
  this.recordEndSound_ = document.createElement('audio');

  /**
   * @type {boolean}
   * @private
   */
  this.locked_ = false;

  /**
   * The main (full screen) canvas for full quality capture.
   * @type {fx.Canvas}
   * @private
   */
  this.mainCanvas_ = null;

  /**
   * Texture for the full quality canvas.
   * @type {fx.Texture}
   * @private
   */
  this.mainCanvasTexture_ = null;

  /**
   * The main (full screen) canvas for previewing capture.
   * @type {fx.Canvas}
   * @private
   */
  this.mainPreviewCanvas_ = null;

  /**
   * Texture for the previewing canvas.
   * @type {fx.Texture}
   * @private
   */
  this.mainPreviewCanvasTexture_ = null;

  /**
   * The main (full screen canvas) for fast capture.
   * @type {fx.Canvas}
   * @private
   */
  this.mainFastCanvas_ = null;

  /**
   * Texture for the fast canvas.
   * @type {fx.Texture}
   * @private
   */
  this.mainFastCanvasTexture_ = null;

  /**
   * The main (full screen) processor in the full quality mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainProcessor_ = null;

  /**
   * The main (full screen) processor in the previewing mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainPreviewProcessor_ = null;

  /**
   * The main (full screen) processor in the fast mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainFastProcessor_ = null;

  /**
   * Current previewing frame.
   * @type {number}
   * @private
   */
  this.frame_ = 0;

  /**
   * If the toolbar is expanded.
   * @type {boolean}
   * @private
   */
  this.expanded_ = false;

  /**
   * Toolbar animation effect wrapper.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.toolbarEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        var toggleFilters = document.querySelector('#filters-toggle');
        if (toggleFilters.disabled) {
          return;
        }
        var toolbar = document.querySelector('#toolbar');
        if (args) {
          toolbar.classList.add('expanded');
        } else {
          toolbar.classList.remove('expanded');
          // Make all of the effects non-focusable.
          var elements = document.querySelectorAll('#effects li');
          for (var index = 0; index < elements.length; index++) {
            elements[index].tabIndex = -1;
          }
          // If something was focused before, then focus the toggle button.
          if (document.activeElement != document.body) {
            toggleFilters.focus();
          }
        }
        camera.util.waitForTransitionCompletion(
            document.querySelector('#toolbar'), 500, function() {
          callback();
        });
      }.bind(this));

  /**
   * Timer for hiding the toast message after some delay.
   * @type {number?}
   * @private
   */
  this.toastHideTimer_ = null;

  /**
   * Toast transition wrapper. Shows or hides the toast with the passed message.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.toastEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        var toastElement = document.querySelector('#toast');
        var toastMessageElement = document.querySelector('#toast-message');
        // Hide the message if visible.
        if (!args.visible && toastElement.classList.contains('visible')) {
          toastElement.classList.remove('visible');
          camera.util.waitForTransitionCompletion(
              toastElement, 500, callback);
        } else if (args.visible) {
          // If showing requested, then show.
          toastMessageElement.textContent = args.message;
          toastElement.classList.add('visible');
          camera.util.waitForTransitionCompletion(
             toastElement, 500, callback);
        } else {
          callback();
        }
      }.bind(this));

  /**
   * Timer for elapsed recording time for video recording.
   * @type {number?}
   * @private
   */
  this.recordingTimer_ = null;

  /**
   * Whether a picture is being taken. Used for updating toolbar UI.
   * @type {boolean}
   * @private
   */
  this.taking_ = false;

  /**
   * Timer used to automatically collapse the tools.
   * @type {?number}
   * @private
   */
  this.collapseTimer_ = null;

  /**
   * Set to true before the ribbon is displayed. Used to render the ribbon's
   * frames while it is not yet displayed, so the previews have some image
   * as soon as possible.
   * @type {boolean}
   * @private
   */
  this.ribbonInitialization_ = true;

  /**
   * Whether the camera is in video recording mode.
   * @type {boolean}
   * @private
   */
  this.is_recording_mode_ = false;

  /**
   * Scroller for the ribbon with effects.
   * @type {camera.util.SmoothScroller}
   * @private
   */
  this.scroller_ = new camera.util.SmoothScroller(
      document.querySelector('#effects'),
      document.querySelector('#effects .padder'));

  /**
   * Scroll bar for the ribbon with effects.
   * @type {camera.HorizontalScrollBar}
   * @private
   */
  this.scrollBar_ = new camera.HorizontalScrollBar(this.scroller_);

  /**
   * Detects if the mouse has been moved or clicked, and if any touch events
   * have been performed on the view. If such events are detected, then the
   * ribbon and the window buttons are shown.
   *
   * @type {camera.util.PointerTracker}
   * @private
   */
  this.pointerTracker_ = new camera.util.PointerTracker(
      document.body, this.onPointerActivity_.bind(this));

  /**
   * Enables scrolling the ribbon by dragging the mouse.
   * @type {camera.util.MouseScroller}
   * @private
   */
  this.mouseScroller_ = new camera.util.MouseScroller(this.scroller_);

  /**
   * Detects whether scrolling is being performed or not.
   * @type {camera.util.ScrollTracker}
   * @private
   */
  this.scrollTracker_ = new camera.util.ScrollTracker(
      this.scroller_, function() {}, function() {});

  /**
   * @type {string}
   * @private
   */
  this.keyBuffer_ = '';

  /**
   * Measures performance.
   * @type {camera.util.NamedPerformanceMonitor}
   * @private
   */
  this.performanceMonitors_ = new camera.util.NamedPerformanceMonitors();

  /**
   * Timer used for a multi-shot.
   * @type {number?}
   * @private
   */
  this.multiShotInterval_ = null;

  /**
   * Timer used to countdown before taking the picture.
   * @type {number?}
   * @private
   */
  this.takePictureTimer_ = null;

  /**
   * DeviceId of the camera device used for the last time during this session.
   * @type {string?}
   * @private
   */
  this.videoDeviceId_ = null;

  /**
   * Whether list of video devices is being refreshed now.
   * @type {boolean}
   * @private
   */
  this.refreshingVideoDeviceIds_ = false;

  /**
   * List of available video device ids.
   * @type {Promise<!Array<string>>}
   * @private
   */
  this.videoDeviceIds_ = null;

  /**
   * Legacy mirroring set per all devices.
   * @type {boolean}
   * @private
   */
  this.legacyMirroringToggle_ = true;

  /**
   * Mirroring set per device.
   * @type {!Object}
   * @private
   */
  this.mirroringToggles_ = {};

  /**
   * Aspect ratio of the last opened video stream, or null if nothing was
   * opened.
   * @type {?number}
   */
  this.lastVideoAspectRatio_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  // If dimensions of the video are first known or changed, then synchronize
  // the window bounds.
  this.video_.addEventListener('loadedmetadata',
      this.synchronizeBounds_.bind(this));
  this.video_.addEventListener('resize',
      this.synchronizeBounds_.bind(this));

  // Handle the 'Take' button.
  document.querySelector('#take-picture').addEventListener(
      'click', this.onTakePictureClicked_.bind(this));

  document.querySelector('#toolbar #album-enter').addEventListener('click',
      this.onAlbumEnterClicked_.bind(this));

  document.querySelector('#toolbar #filters-toggle').addEventListener('click',
      this.onFiltersToggleClicked_.bind(this));

  window.addEventListener('keydown', this.onWindowKeyDown_.bind(this));

  document.querySelector('#toggle-record').addEventListener(
      'click', this.onToggleRecordClicked_.bind(this));
  document.querySelector('#toggle-timer').addEventListener(
      'keypress', this.onToggleTimerKeyPress_.bind(this));
  document.querySelector('#toggle-timer').addEventListener(
      'click', this.onToggleTimerClicked_.bind(this));
  document.querySelector('#toggle-multi').addEventListener(
      'keypress', this.onToggleMultiKeyPress_.bind(this));
  document.querySelector('#toggle-multi').addEventListener(
      'click', this.onToggleMultiClicked_.bind(this));
  document.querySelector('#toggle-camera').addEventListener(
      'click', this.onToggleCameraClicked_.bind(this));
  document.querySelector('#toggle-mirror').addEventListener(
      'keypress', this.onToggleMirrorKeyPress_.bind(this));
  document.querySelector('#toggle-mirror').addEventListener(
      'click', this.onToggleMirrorClicked_.bind(this));

  // Load the shutter, tick, and recording sound.
  this.shutterSound_.src = '../sounds/shutter.ogg';
  this.tickSound_.src = '../sounds/tick.ogg';
  this.recordStartSound_.src = '../sounds/record_start.ogg';
  this.recordEndSound_.src = '../sounds/record_end.ogg';
};

/**
 * Frame draw mode.
 * @enum {number}
 */
camera.views.Camera.DrawMode = Object.freeze({
  FAST: 0,  // Quality optimized for best performance.
  NORMAL: 1,  // Quality adapted to the window's current size.
  BEST: 2  // The best quality possible.
});

/**
 * Ratio between video and window aspect ratios to be considered close enough
 * to snap the window dimensions to the window frame. 1.1 means the aspect
 * ratios can differ by 1.1 times.
 *
 * @type {number}
 * @const
 */
camera.views.Camera.ASPECT_RATIO_SNAP_RANGE = 1.1;

camera.views.Camera.prototype = {
  __proto__: camera.View.prototype,
  get capturing() {
    return document.body.classList.contains('capturing');
  }
};

/**
 * Initializes the view.
 * @override
 */
camera.views.Camera.prototype.initialize = function(callback) {
  // Initialize the webgl canvases.
  try {
    this.mainCanvas_ = fx.canvas();
    this.mainPreviewCanvas_ = fx.canvas();
    this.mainFastCanvas_ = fx.canvas();
  }
  catch (e) {
    // Initialization failed due to lack of webgl.
    // TODO(mtomasz): Replace with a better icon.
    this.context_.onError('no-camera',
        chrome.i18n.getMessage('errorMsgNoWebGL'),
        chrome.i18n.getMessage('errorMsgNoWebGLHint'));
  }

  if (this.mainCanvas_ && this.mainPreviewCanvas_ && this.mainFastCanvas_) {
    // Initialize the processors.
    this.mainCanvasTexture_ = this.mainCanvas_.texture(this.video_);
    this.mainPreviewCanvasTexture_ = this.mainPreviewCanvas_.texture(
        this.video_);
    this.mainFastCanvasTexture_ = this.mainFastCanvas_.texture(this.video_);
    this.mainProcessor_ = new camera.Processor(
        this.mainCanvasTexture_,
        this.mainCanvas_,
        this.mainCanvas_);
    this.mainPreviewProcessor_ = new camera.Processor(
        this.mainPreviewCanvasTexture_,
        this.mainPreviewCanvas_,
        this.mainPreviewCanvas_);
    this.mainFastProcessor_ = new camera.Processor(
        this.mainFastCanvasTexture_,
        this.mainFastCanvas_,
        this.mainFastCanvas_);

    // Insert the main canvas to its container.
    document.querySelector('#main-canvas-wrapper').appendChild(
        this.mainCanvas_);
    document.querySelector('#main-preview-canvas-wrapper').appendChild(
        this.mainPreviewCanvas_);
    document.querySelector('#main-fast-canvas-wrapper').appendChild(
        this.mainFastCanvas_);

    // Select the default state of the toggle buttons.
    // TODO(mtomasz): Move to chrome.storage.local.sync, after implementing
    // syncing of the gallery.
    chrome.storage.local.get(
        {
          toggleTimer: false,
          toggleMulti: false,
          toggleMirror: true,  // Deprecated.
          mirroringToggles: {},  // Per device.
        },
        function(values) {
          document.querySelector('#toggle-timer').checked =
              values.toggleTimer;
          document.querySelector('#toggle-multi').checked =
              values.toggleMulti;
          this.legacyMirroringToggle_ = values.toggleMirror;
          this.mirroringToggles_ = values.mirroringToggles;

          // Initialize the web camera.
          this.start_();
        }.bind(this));
    // Remove the deprecated values.
    chrome.storage.local.remove(['effectIndex']);
  }

  // TODO: Replace with "devicechanged" event once it's implemented in Chrome.
  this.maybeRefreshVideoDeviceIds_();
  setInterval(this.maybeRefreshVideoDeviceIds_.bind(this), 1000);

  // Monitor the locked state to avoid retrying camera connection when locked.
  chrome.idle.onStateChanged.addListener(function(newState) {
    if (newState == 'locked')
      this.locked_ = true;
    else if (newState == 'active')
      this.locked_ = false;
  }.bind(this));

  // Acquire the gallery model.
  camera.models.Gallery.getInstance(function(model) {
    this.model_ = model;
    this.model_.addObserver(this);
    this.updateAlbumButton_();
    callback();
  }.bind(this), function() {
    // TODO(mtomasz): Add error handling.
    console.error('Unable to initialize the file system.');
    callback();
  });
};

/**
 * Enters the view.
 * @override
 */
camera.views.Camera.prototype.onEnter = function() {
  this.performanceMonitors_.reset();
  this.mainProcessor_.performanceMonitors.reset();
  this.mainPreviewProcessor_.performanceMonitors.reset();
  this.mainFastProcessor_.performanceMonitors.reset();
  this.onResize();
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Camera.prototype.onLeave = function() {
};

/**
 * @override
 */
camera.views.Camera.prototype.onActivate = function() {
  this.scrollTracker_.start();
  if (document.activeElement != document.body)
    document.querySelector('#take-picture').focus();
  this.updateAlbumButton_();
};

/**
 * @override
 */
camera.views.Camera.prototype.onInactivate = function() {
  this.setExpanded_(false);
  this.scrollTracker_.stop();
  if (this.taking_) {
    this.endTakePicture_();
  }
};

/**
 * Handles clicking on the take-picture button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onTakePictureClicked_ = function(event) {
  if (this.is_recording_mode_ && this.mediaRecorder_ == null) {
    // Create a media recorder before proceeding to record video.
    this.mediaRecorder_ = this.createMediaRecorder_(this.stream_);
    if (this.mediaRecorder_ == null) {
        this.showToastMessage_(chrome.i18n.getMessage(
            'errorMsgRecordStartFailed'));
        return;
    }
  }
  this.takePicture_();
};

/**
 * Handles clicking on the album button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onAlbumEnterClicked_ = function(event) {
  this.router.navigate(camera.Router.ViewIdentifier.ALBUM);
};

/**
 * Handles clicking on the toggle filters button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onFiltersToggleClicked_ = function(event) {
  this.setExpanded_(!this.expanded_);
};

/**
 * Handles pressing a key within a window.
 * TODO(yuli): Remove this function when removing effects UI.
 *
 * @param {Event} event Key down event
 * @private
 */
camera.views.Camera.prototype.onWindowKeyDown_ = function(event) {
  // When the ribbon is focused, then do not collapse it when pressing keys.
  if (this.active &&
      document.activeElement == document.querySelector('#effects-wrapper')) {
    this.setExpanded_(true);
  }
};

/**
 * Handles pressing a key on the timer switch.
 * @param {Event} event Key press event.
 * @private
 */
camera.views.Camera.prototype.onToggleTimerKeyPress_ = function(event) {
  if (camera.util.getShortcutIdentifier(event) == 'Enter')
    document.querySelector('#toggle-timer').click();
};

/**
 * Handles pressing a key on the multi-shot switch.
 * @param {Event} event Key press event.
 * @private
 */
camera.views.Camera.prototype.onToggleMultiKeyPress_ = function(event) {
  if (camera.util.getShortcutIdentifier(event) == 'Enter')
    document.querySelector('#toggle-multi').click();
};

/**
 * Handles pressing a key on the mirror switch.
 * @param {Event} event Key press event.
 * @private
 */
camera.views.Camera.prototype.onToggleMirrorKeyPress_ = function(event) {
  if (camera.util.getShortcutIdentifier(event) == 'Enter')
    document.querySelector('#toggle-mirror').click();
};

/**
 * Handles clicking on the timer switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleTimerClicked_ = function(event) {
  var enabled = document.querySelector('#toggle-timer').checked;
  this.showToastMessage_(
      chrome.i18n.getMessage(enabled ? 'toggleTimerActiveMessage' :
                                       'toggleTimerInactiveMessage'));
  chrome.storage.local.set({toggleTimer: enabled});
};

/**
 * Handles clicking on the multi-shot switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleMultiClicked_ = function(event) {
  var enabled = document.querySelector('#toggle-multi').checked;
  this.showToastMessage_(
      chrome.i18n.getMessage(enabled ? 'toggleMultiActiveMessage' :
                                       'toggleMultiInactiveMessage'));
  chrome.storage.local.set({toggleMulti: enabled});
};

/**
 * Handles clicking on the toggle camera switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleCameraClicked_ = function(event) {
  this.videoDeviceIds_.then(function(deviceIds) {
    var index = deviceIds.indexOf(this.videoDeviceId_);
    if (index == -1)
      index = 0;

    if (deviceIds.length > 0) {
      index = (index + 1) % deviceIds.length;
      this.videoDeviceId_ = deviceIds[index];
    }

    this.stop_();
  }.bind(this));
};

/**
 * Handles clicking on the video-recording switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleRecordClicked_ = function(event) {
  var label;
  var toggleRecord = document.querySelector('#toggle-record');
  if (this.is_recording_mode_) {
    this.is_recording_mode_ = false;
    toggleRecord.classList.remove('toggle-off');
    label = 'toggleRecordOnButton';
  } else {
    this.is_recording_mode_ = true;
    toggleRecord.classList.add('toggle-off');
    label = 'toggleRecordOffButton';
  }
  this.updateButtonLabel_(toggleRecord, label);

  var takePictureButton = document.querySelector('#take-picture');
  if (this.is_recording_mode_) {
    takePictureButton.classList.add('motion-picture');
    label = 'recordVideoStartButton';
  } else {
    takePictureButton.classList.remove('motion-picture');
    label = 'takePictureButton';
  }
  this.updateButtonLabel_(takePictureButton, label);

  document.querySelector('#toggle-multi').hidden = this.is_recording_mode_;
  document.querySelector('#toggle-timer').hidden = this.is_recording_mode_;

  this.stop_();
};

/**
 * Handles clicking on the mirror switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleMirrorClicked_ = function(event) {
  var enabled = document.querySelector('#toggle-mirror').checked;
  this.showToastMessage_(
      chrome.i18n.getMessage(enabled ? 'toggleMirrorActiveMessage' :
                                       'toggleMirrorInactiveMessage'));
  this.mirroringToggles_[this.videoDeviceId_] = enabled;
  chrome.storage.local.set({mirroringToggles: this.mirroringToggles_});
  this.updateMirroring_();
};

/**
 * Handles pointer actions, such as mouse or touch activity.
 * @param {Event} event Activity event.
 * @private
 */
camera.views.Camera.prototype.onPointerActivity_ = function(event) {
  // Update the ribbon's visibility only when camera view is active.
  // TODO(yuli): Remove this function after removing effects UI.
  if (this.active) {
    switch (event.type) {
      case 'mousedown':
        // Toggle the ribbon if clicking on static area.
        if (event.target == document.body ||
            document.querySelector('#main-canvas-wrapper').contains(
                event.target) ||
            document.querySelector('#main-preview-canvas-wrapper').contains(
                event.target) ||
            document.querySelector('#main-fast-canvas-wrapper').contains(
                event.target)) {
          this.setExpanded_(!this.expanded_);
          break;
        }  // Otherwise continue.
      default:
        // Prevent auto-hiding the ribbon for any other activity.
        if (this.expanded_)
          this.setExpanded_(true);
        break;
    }
  }
};

/**
 * Updates the UI to reflect the mirroring either set automatically or by user.
 * @private
 */
camera.views.Camera.prototype.updateMirroring_ = function() {
  var toggleMirror = document.querySelector('#toggle-mirror')
  var enabled;

  var track = this.stream_ && this.stream_.getVideoTracks()[0];
  var trackSettings = track.getSettings && track.getSettings();
  var facingMode = trackSettings && trackSettings.facingMode;

  toggleMirror.hidden = !!facingMode;

  if (facingMode) {
    // Automatic mirroring detection.
    enabled = facingMode == 'user';
  } else {
    // Manual mirroring.
    if (this.videoDeviceId_ in this.mirroringToggles_)
      enabled = this.mirroringToggles_[this.videoDeviceId_];
    else
      enabled = this.legacyMirroringToggle_;
  }

  toggleMirror.checked = enabled;
  document.body.classList.toggle('mirror', enabled);
};

/**
 * Updates the album-button enabled/disabled UI for model changes.
 * @private
 */
camera.views.Camera.prototype.updateAlbumButton_ = function() {
  // Album-button would also be disabled if camera isn't capturing.
  document.querySelector('#album-enter').disabled =
      !this.model_ || this.model_.length == 0 || !this.capturing;
};

/**
 * Updates the toolbar enabled/disabled UI for capturing or taking-picture
 * state changes.
 * @private
 */
camera.views.Camera.prototype.updateToolbar_ = function() {
  var disabled = !this.capturing || this.taking_;
  document.querySelector('#toggle-timer').disabled = disabled;
  document.querySelector('#toggle-multi').disabled = disabled;
  document.querySelector('#toggle-mirror').disabled = disabled;
  document.querySelector('#toggle-camera').disabled = disabled;
  document.querySelector('#toggle-record').disabled = disabled;
  document.querySelector('#take-picture').disabled = disabled;

  this.updateAlbumButton_();
};

/**
 * Updates the button's labels.
 *
 * @param {HTMLElement} button Button element to be updated.
 * @param {string} label Label to be set.
 * @private
 */
camera.views.Camera.prototype.updateButtonLabel_ = function(button, label) {
  button.setAttribute('i18n-label', label);
  button.setAttribute('aria-label', chrome.i18n.getMessage(label));
};

/**
 * Enables the audio track for audio capturing.
 *
 * @param {boolean} enabled True to enable audio track, false to disable.
 * @private
 */
camera.views.Camera.prototype.enableAudio_ = function(enabled) {
  var track = this.stream_ && this.stream_.getAudioTracks()[0];
  if (track) {
    track.enabled = enabled;
  }
};

/**
 * @override
 */
camera.views.Camera.prototype.onResize = function() {
  this.synchronizeBounds_();
};

/**
 * @override
 */
camera.views.Camera.prototype.onKeyPressed = function(event) {
  this.keyBuffer_ += String.fromCharCode(event.which);
  this.keyBuffer_ = this.keyBuffer_.substr(-10);

  // Allow to load a file stream (for debugging).
  if (this.keyBuffer_.indexOf('CRAZYPONY') !== -1) {
    this.chooseFileStream_();
    this.keyBuffer_ = '';
  }

  if (this.keyBuffer_.indexOf('VER') !== -1) {
    this.showVersion_();
    this.printPerformanceStats_();
    this.keyBuffer_ = '';
  }

  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Space':  // Space key for taking the picture.
      document.querySelector('#take-picture').click();
      event.stopPropagation();
      event.preventDefault();
      break;
    case 'G':  // G key for the gallery.
      document.querySelector('#album-enter').click();
      event.preventDefault();
      break;
  }
};

/**
 * Shows a non-intrusive toast message in the middle of the screen.
 * @param {string} message Message to be shown.
 * @private
 */
camera.views.Camera.prototype.showToastMessage_ = function(message) {
  var cancelHideTimer = function() {
    if (this.toastHideTimer_) {
      clearTimeout(this.toastHideTimer_);
      this.toastHideTimer_ = null;
    }
  }.bind(this);

  // If running, then reinvoke recursively after closing the toast message.
  if (this.toastEffect_.animating || this.toastHideTimer_) {
    cancelHideTimer();
    this.toastEffect_.invoke({
      visible: false
    }, this.showToastMessage_.bind(this, message));
    return;
  }

  // Cancel any pending hide timers.
  cancelHideTimer();

  // Start the hide timer.
  this.toastHideTimer_ = setTimeout(function() {
    this.toastEffect_.invoke({
      visible: false
    }, function() {});
    this.toastHideTimer_ = null;
  }.bind(this), 2000);

  // Show the toast message.
  this.toastEffect_.invoke({
    visible: true,
    message: message
  }, function() {});
};

/**
 * Starts the timer and shows it on screen.
 * @private
 */
camera.views.Camera.prototype.showRecordingTimer_ = function() {
  // Format number of seconds into "HH:MM:SS" string.
  var formatSeconds = function(seconds) {
    var sec = seconds % 60;
    var min = Math.floor(seconds / 60) % 60;
    var hr = Math.floor(seconds / 3600);
    if (sec < 10) {
      sec = '0' + sec;
    }
    if (min < 10) {
      min = '0' + min;
    }
    if (hr < 10) {
      hr = '0' + hr;
    }
    return hr + ':' + min + ':' + sec;
  }

  var timerElement = document.querySelector('#recording-timer');
  timerElement.classList.add('visible');
  var timerTicks = 0;
  timerElement.textContent = formatSeconds(timerTicks);

  this.recordingTimer_ = setInterval(function() {
    timerTicks++;
    timerElement.textContent = formatSeconds(timerTicks);
  }, 1000);
};

/**
 * Stops and hides the timer.
 * @private
 */
camera.views.Camera.prototype.hideRecordingTimer_ = function() {
  if (this.recordingTimer_) {
    clearInterval(this.recordingTimer_);
    this.recordingTimer_ = null;
  }

  var timerElement = document.querySelector('#recording-timer');
  timerElement.textContent = '';
  timerElement.classList.remove('visible');
};

/**
 * Toggles the toolbar visibility. However, it may delay the operation, if
 * eg. some UI element is hovered.
 *
 * @param {boolean} expanded True to show the toolbar, false to hide.
 * @private
 */
camera.views.Camera.prototype.setExpanded_ = function(expanded) {
  if (this.collapseTimer_) {
    clearTimeout(this.collapseTimer_);
    this.collapseTimer_ = null;
  }
  // Don't expand to show the effects if they are not supported.
  if (!document.querySelector('#filters-toggle').disabled &&
      expanded) {
    var isRibbonHovered =
        document.querySelector('#toolbar').webkitMatchesSelector(':hover');
    if (!isRibbonHovered) {
      this.collapseTimer_ = setTimeout(
          this.setExpanded_.bind(this, false), 3000);
    }
    if (!this.expanded_) {
      this.toolbarEffect_.invoke(true, function() {
        this.expanded_ = true;
      }.bind(this));
    }
  } else {
    if (this.expanded_) {
      this.expanded_ = false;
      this.toolbarEffect_.invoke(false, function() {});
    }
  }
};

/**
 * Chooses a file stream to override the camera stream. Used for debugging.
 * @private
 */
camera.views.Camera.prototype.chooseFileStream_ = function() {
  chrome.fileSystem.chooseEntry(function(fileEntry) {
    if (!fileEntry)
      return;
    fileEntry.file(function(file) {
      var url = URL.createObjectURL(file);
      this.video_.src = url;
      this.video_.play();
    }.bind(this));
  }.bind(this));
};

/**
 * Shows a version dialog.
 * @private
 */
camera.views.Camera.prototype.showVersion_ = function() {
  // No need to localize, since for debugging purpose only.
  var message = 'Version: ' + chrome.runtime.getManifest().version + '\n' +
      'Resolution: ' +
          this.video_.videoWidth + 'x' + this.video_.videoHeight + '\n' +
      'Frames per second: ' +
          this.performanceMonitors_.fps('main').toPrecision(2) + '\n';
  this.router.navigate(camera.Router.ViewIdentifier.DIALOG, {
    type: camera.views.Dialog.Type.ALERT,
    message: message
  });
};

/**
 * Takes the picture (maybe with a timer if enabled); or ends an ongoing
 * recording started from the prior takePicture_() call.
 * @private
 */
camera.views.Camera.prototype.takePicture_ = function() {
  if (this.taking_) {
    // End the prior ongoing taking/recording; a new taking/reocording won't be
    // started until the prior one is ended.
    this.endTakePicture_();
    return;
  }

  this.taking_ = true;
  this.updateToolbar_();

  var toggleTimer = document.querySelector('#toggle-timer');
  var toggleMulti = document.querySelector('#toggle-multi');

  var tickCounter = (!toggleTimer.hidden && toggleTimer.checked) ? 6 : 1;
  var onTimerTick = function() {
    tickCounter--;
    if (tickCounter == 0) {
      var multiEnabled = !toggleMulti.hidden && toggleMulti.checked;
      var multiShotCounter = 3;
      var takePicture = function() {
        if (this.is_recording_mode_) {
          // Play a sound before recording started, and don't end recording
          // until another take-picture click.
          this.recordStartSound_.currentTime = 0;
          this.recordStartSound_.play();
          setTimeout(function() {
            // Record-started sound should have been ended by now; pause it just
            // in case it's still playing to avoid the sound being recorded.
            this.recordStartSound_.pause();
            this.takePictureImmediately_(true);
          }.bind(this), 250);
        } else {
          this.takePictureImmediately_(false);

          if (multiEnabled) {
            multiShotCounter--;
            if (multiShotCounter)
              return;
          }
          this.endTakePicture_();
        }
      }.bind(this);
      takePicture();
      if (multiEnabled)
        this.multiShotInterval_ = setInterval(takePicture, 250);
    } else {
      this.takePictureTimer_ = setTimeout(onTimerTick, 1000);
      this.tickSound_.play();
      // Blink the toggle timer button.
      toggleTimer.classList.add('animate');
      setTimeout(function() {
        if (this.takePictureTimer_)
          toggleTimer.classList.remove('animate');
      }.bind(this), 500);
    }
  }.bind(this);

  // First tick immediately in the next message loop cycle.
  this.takePictureTimer_ = setTimeout(onTimerTick, 0);
};

/**
 * Ends ongoing recording or clears scheduled further picture takes (if any).
 * @private
 */
camera.views.Camera.prototype.endTakePicture_ = function() {
  if (this.takePictureTimer_) {
    clearTimeout(this.takePictureTimer_);
    this.takePictureTimer_ = null;
  }
  if (this.multiShotInterval_) {
    clearTimeout(this.multiShotInterval_);
    this.multiShotInterval_ = null;
  }
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }
  document.querySelector('#toggle-timer').classList.remove('animate');

  this.taking_ = false;
  this.updateToolbar_();
};

/**
 * Takes the still picture or starts to take the motion picture immediately,
 * and saves and puts to the album.
 *
 * @param {boolean} motionPicture True to start video recording, false to take
 *     a still picture.
 * @private
 */
camera.views.Camera.prototype.takePictureImmediately_ = function(motionPicture) {
  setTimeout(function() {
    this.drawCameraFrame_(camera.views.Camera.DrawMode.BEST);

    // Add picture to the gallery.
    var addPicture = function(blob, type) {
      var saveFailure = function() {
        this.showToastMessage_(
            chrome.i18n.getMessage('errorMsgGallerySaveFailed'));
      }.bind(this);

      if (!this.model_) {
        saveFailure();
        return;
      }
      var albumButton = document.querySelector('#toolbar #album-enter');
      camera.util.setAnimationClass(albumButton, albumButton, 'flash');

      // Add the photo or video to the model.
      this.model_.addPicture(blob, type, saveFailure);
    }.bind(this);

    if (motionPicture) {
      var recordedChunks = [];
      this.mediaRecorder_.ondataavailable = function(event) {
        if (event.data && event.data.size > 0) {
          recordedChunks.push(event.data);
        }
      }.bind(this);

      var takePictureButton = document.querySelector('#take-picture');
      this.mediaRecorder_.onstop = function(event) {
        this.hideRecordingTimer_();
        takePictureButton.classList.remove('flash');
        this.updateButtonLabel_(takePictureButton, 'recordVideoStartButton');
        this.enableAudio_(false);
        // Add the motion picture after the recording is ended.
        // TODO(yuli): Handle insufficient storage.
        var recordedBlob = new Blob(recordedChunks, {type: 'video/webm'});
        recordedChunks = [];
        if (recordedBlob.size) {
          // Play a video-record-ended sound.
          this.recordEndSound_.currentTime = 0;
          this.recordEndSound_.play();
          addPicture(recordedBlob, camera.models.Gallery.PictureType.MOTION);
        } else {
          // The recording may have no data available because it's too short
          // or the media recorder is not stable and Chrome needs to restart.
          this.showToastMessage_(
              chrome.i18n.getMessage('errorMsgEmptyRecording'));
        }
      }.bind(this);

      // Start recording.
      this.enableAudio_(true);
      this.mediaRecorder_.start();

      // Start to show the elapsed recording time.
      this.showRecordingTimer_();

      // Re-enable the take-picture button to stop recording later and flash
      // the take-picture button until the recording is stopped.
      takePictureButton.disabled = false;
      takePictureButton.classList.add('flash');
      this.updateButtonLabel_(takePictureButton, 'recordVideoStopButton');
    } else {
      this.mainCanvas_.toBlob(function(blob) {
        // Play a shutter sound.
        this.shutterSound_.currentTime = 0;
        this.shutterSound_.play();
        addPicture(blob, camera.models.Gallery.PictureType.STILL);
      }.bind(this), 'image/jpeg');
    }
  }.bind(this), 0);
};

/**
 * Synchronizes video size with the window's current size.
 * @private
 */
camera.views.Camera.prototype.synchronizeBounds_ = function() {
  if (!this.video_.videoHeight)
    return;

  var videoRatio = this.video_.videoWidth / this.video_.videoHeight;
  var bodyRatio = document.body.offsetWidth / document.body.offsetHeight;

  var scale;
  if (videoRatio > bodyRatio) {
    scale = Math.min(1, document.body.offsetHeight / this.video_.videoHeight)
    document.body.classList.add('letterbox');
  } else {
    scale = Math.min(1, document.body.offsetWidth / this.video_.videoWidth);
    document.body.classList.remove('letterbox');
  }

  this.mainPreviewProcessor_.scale = scale;
  this.mainFastProcessor_.scale = scale / 2;

  this.video_.width = this.video_.videoWidth;
  this.video_.height = this.video_.videoHeight;
}

/**
 * Creates the media recorder for the video stream.
 *
 * @param {MediaStream} stream Media stream to be recorded.
 * @return {MediaRecorder} Media recorder created.
 * @private
 */
camera.views.Camera.prototype.createMediaRecorder_ = function(stream) {
  if (!window.MediaRecorder) {
    return null;
  }

  // Webm with H264 is the only preferred MediaRecorder video recording format.
  var type = 'video/webm; codecs=h264';
  if (!MediaRecorder.isTypeSupported(type)) {
    console.error('MediaRecorder does not support mimeType: ' + type);
    return null;
  }
  try {
    var options = {mimeType: type};
    return new MediaRecorder(stream, options);
  } catch (e) {
    console.error('Unable to create MediaRecorder: ' + e + '. mimeType: ' +
        type);
    return null;
  }
};

/**
 * Starts capturing with the specified constraints.
 *
 * @param {!Object} constraints Constraints passed to WebRTC.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback, eg. the constraints are
 *     not supported.
 * @private
 */
 camera.views.Camera.prototype.startWithConstraints_ =
     function(constraints, onSuccess, onFailure) {
  navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
    if (this.is_recording_mode_) {
      // Disable audio stream until video recording is started.
      this.enableAudio_(false);
    }
    // Mute to avoid echo from the captured audio.
    this.video_.muted = true;
    this.video_.srcObject = stream;
    var onLoadedMetadata = function() {
      this.video_.removeEventListener('loadedmetadata', onLoadedMetadata);
      // Use a watchdog since the stream.onended event is unreliable in the
      // recent version of Chrome. As of 55, the event is still broken.
      this.watchdog_ = setInterval(function() {
        // Check if video stream is ended (audio stream may still be live).
        if (!stream.getVideoTracks().length ||
            stream.getVideoTracks()[0].readyState == 'ended') {
          clearInterval(this.watchdog_);
          this.watchdog_ = null;
          if (this.taking_) {
            this.endTakePicture_();
          }
          if (this.mediaRecorder_) {
            this.mediaRecorder_ = null;
          }
          // Try reconnecting the camera to capture new streams.
          document.body.classList.remove('capturing');
          this.updateToolbar_();
          this.stream_ = null;
          this.start_();
        }
      }.bind(this), 100);

      this.stream_ = stream;
      document.body.classList.add('capturing');
      this.updateToolbar_();
      var onAnimationFrame = function() {
        this.onAnimationFrame_();
        requestAnimationFrame(onAnimationFrame);
      }.bind(this);
      onAnimationFrame();
      onSuccess();
    }.bind(this);
    // Load the stream and wait for the metadata.
    this.video_.addEventListener('loadedmetadata', onLoadedMetadata);
    this.video_.play();
  }.bind(this), function(error) {
    if (error && error.name != 'ConstraintNotSatisfiedError') {
      // Constraint errors are expected, so don't report them.
      console.error(error);
    }
    onFailure();
  });
};

/**
 * Sets the window size to match the video frame aspect ratio, and positions
 * it to stay visible and opticallly appealing.
 * @param {number=} opt_maxOuterHeight Maximum height of the window to be
 *     applied.
 * @private
 */
camera.views.Camera.prototype.resetWindowGeometry_ = function(
    opt_maxOuterHeight) {
  var outerBounds = chrome.app.window.current().outerBounds;
  var innerBounds = chrome.app.window.current().innerBounds;

  var decorationInsets = {
    left: innerBounds.left - outerBounds.left,
    top: innerBounds.top - outerBounds.top,
  };
  decorationInsets.right = outerBounds.width - innerBounds.width -
      decorationInsets.left;
  decorationInsets.bottom = outerBounds.height - innerBounds.height -
      decorationInsets.top;

  // Screen paddings are 10% of the dimension to avoid spanning the window
  // behind any overlay launcher (if exists). However, if the window already
  // exceeds these paddings, then use those paddings, so the user window
  // placement is respected. Eg. if the user moved the window behind the
  // overlay launcher, we shouldn't move it out of there. Though, we shouldn't
  // move the window behind the overlay launcher if it wasn't there before.
  var screenPaddings = {
    left: Math.min(outerBounds.left, 0.1 * screen.width),
    top: Math.min(outerBounds.top, 0.1 * screen.height),
    right: Math.min(0.1 * screen.width, screen.width - outerBounds.width -
        outerBounds.left),
    bottom: Math.min(0.1 * screen.height, screen.height - outerBounds.height -
        outerBounds.top)
  };

  // Moving the window partly outside of the top bound is impossible with
  // a user gesture, though some users ended up with this hard to recover
  // situation due to a bug in Camera app. Explicitly do not allow this
  // situation.
  if (screenPaddings.top < 0) {
    screenPaddings.top = 0;
  }

  // We need aspect ratio of the client are so insets have to be taken into
  // account. Though never make the window larger than the screen size (unless
  // maximized).
  var maxOuterWidth = Math.min(screen.width,
      screen.width - screenPaddings.left - screenPaddings.right);
  var maxOuterHeight = Math.min(screen.height,
      opt_maxOuterHeight != undefined ? opt_maxOuterHeight :
      (screen.height - screenPaddings.top - screenPaddings.bottom));

  var targetAspectRatio = this.video_.videoWidth / this.video_.videoHeight;

  // We need aspect ratio of the client are so insets have to be taken into
  // account.
  var targetWidth = maxOuterWidth;
  var targetHeight =
      (targetWidth - decorationInsets.left - decorationInsets.right) /
      targetAspectRatio + decorationInsets.top + decorationInsets.bottom;

  if (targetHeight > maxOuterHeight) {
    targetHeight = maxOuterHeight;
    targetWidth =
        (targetHeight - decorationInsets.top - decorationInsets.bottom) *
        targetAspectRatio + decorationInsets.left + decorationInsets.right;
  }

  // If dimensions didn't change, then respect the position set previously by
  // the user.
  if (Math.round(targetWidth) == outerBounds.width &&
      Math.round(targetHeight) == outerBounds.height) {
    return;
  }

  // Use center as gravity to expand the dimensions in all directions if
  // possible.
  var targetLeft = outerBounds.left - (targetWidth - outerBounds.width) / 2;
  var targetTop = outerBounds.top - (targetHeight - outerBounds.height) / 2;

  var leftClamped = Math.max(screenPaddings.left, Math.min(
      targetLeft, screen.width - targetWidth - screenPaddings.right));
  var topClamped = Math.max(screenPaddings.top, Math.min(
      targetTop, screen.height - targetHeight - screenPaddings.bottom));

  outerBounds.setPosition(Math.round(leftClamped), Math.round(topClamped));
  outerBounds.setSize(Math.round(targetWidth), Math.round(targetHeight));
};

/**
 * Checks whether the current inner bounds of the window matche the passed
 * aspect ratio closely enough.
 * @param {number} aspectRatio Aspect ratio to compare against.
 * @param {boolean} True if yes, false otherwise.
 */
camera.views.Camera.prototype.isInnerBoundsAlmostAspectRatio_ =
    function(aspectRatio) {
  var bounds = chrome.app.window.current().innerBounds;
  var windowAspectRatio = bounds.width / bounds.height;

  if (aspectRatio / windowAspectRatio <
          camera.views.Camera.ASPECT_RATIO_SNAP_RANGE &&
      windowAspectRatio / aspectRatio <
          camera.views.Camera.ASPECT_RATIO_SNAP_RANGE) {
    return true;
  }

  return false;
};

/**
 * Updates list of available video devices when changed, including the UI.
 * Does nothing if refreshing is already in progress.
 * @private
 */
camera.views.Camera.prototype.maybeRefreshVideoDeviceIds_ = function() {
  if (this.refreshingVideoDeviceIds_)
    return;

  this.refreshingVideoDeviceIds_ = true;
  this.videoDeviceIds_ = this.collectVideoDevices_();

  // Update the UI.
  this.videoDeviceIds_.then(function(devices) {
    document.querySelector('#toggle-camera').hidden = devices.length < 2;
  }, function() {
    document.querySelector('#toggle-camera').hidden = true;
  }).then(function() {
    this.refreshingVideoDeviceIds_ = false;
  }.bind(this));
};

/**
 * Collects all of the available video input devices.
 * @return {!Promise<!Array<string>}
 * @private
 */
camera.views.Camera.prototype.collectVideoDevices_ = function() {
  return navigator.mediaDevices.enumerateDevices().then(function(devices) {
      var availableVideoDevices = [];
      devices.forEach(function(device) {
        if (device.kind != 'videoinput')
          return;
        availableVideoDevices.push(device.deviceId);
      });
      return availableVideoDevices;
    });
};

/**
 * Stops the camera stream so it retries opening the camera stream
 * on new device or with new constraints.
 */
camera.views.Camera.prototype.stop_ = function() {
  // TODO(mtomasz): Prevent blink. Clear somehow the video tag.
  if (this.stream_)
    this.stream_.getVideoTracks()[0].stop();
};

/**
 * Starts capturing the camera with the highest possible resolution.
 * Can be called only once.
 * @private
 */
camera.views.Camera.prototype.start_ = function() {
  var scheduleRetry = function() {
    if (this.retryStartTimer_) {
      clearTimeout(this.retryStartTimer_);
      this.retryStartTimer_ = null;
    }
    this.retryStartTimer_ = setTimeout(this.start_.bind(this), 100);
  }.bind(this);

  if (this.locked_) {
    scheduleRetry();
    return;
  }

  var onSuccess = function() {
    // When no bounds are remembered, then apply some default geometry which
    // matches the video aspect ratio. Otherwise, if the window is very close
    // to the aspect ratio of the last video, then resnap to the new aspect
    // ratio while keeping the last height. This is to avoid changing window
    // size if the user explicitly changed the dimensions to not matching the
    // aspect ratio.
    var bounds = chrome.app.window.current().outerBounds;
    if (bounds.width == 640 && bounds.height == 360) {
      this.resetWindowGeometry_();
    } else if (this.lastVideoAspectRatio_ != null &&
        this.isInnerBoundsAlmostAspectRatio_(this.lastVideoAspectRatio_)) {
      this.resetWindowGeometry_(bounds.height);
    }

    this.lastVideoAspectRatio_ = this.video_.width / this.video_.height;

    // Set the ribbon in the initialization mode for 500 ms. This forces repaint
    // of the ribbon, even if it is hidden, or animations are in progress.
    setTimeout(function() {
      this.ribbonInitialization_ = false;
    }.bind(this), 500);

    if (this.retryStartTimer_) {
      clearTimeout(this.retryStartTimer_);
      this.retryStartTimer_ = null;
    }
    // Remove the error layer if any.
    this.context_.onErrorRecovered('no-camera');

    this.showToastMessage_(chrome.i18n.getMessage(this.is_recording_mode_ ?
        'recordVideoActiveMessage' : 'takePictureActiveMessage'));
  }.bind(this);

  var onFailure = function() {
    this.context_.onError(
        'no-camera',
        chrome.i18n.getMessage('errorMsgNoCamera'),
        chrome.i18n.getMessage('errorMsgNoCameraHint'));
    scheduleRetry();
  }.bind(this);

  var constraintsCandidates = [];

  var tryStartWithConstraints = function(index) {
    if (this.locked_) {
      scheduleRetry();
      return;
    }
    if (index >= constraintsCandidates.length) {
      onFailure();
      return;
    }
    this.startWithConstraints_(
        constraintsCandidates[index],
        function() {
          if (constraintsCandidates[index].video.deviceId) {
            // For non-default cameras fetch the deviceId from constraints.
            // Works on all supported Chrome versions.
            this.videoDeviceId_ =
                constraintsCandidates[index].video.deviceId.exact;
          } else {
            // For default camera, obtain the deviceId from settings, which is
            // a feature available only from 59. For older Chrome versions,
            // it's impossible to detect the device id. As a result, if the
            // default camera was changed to rear in chrome://settings, then
            // toggling the camera may not work when pressed for the first time
            // (the same camera would be opened).
            var track = this.stream_.getVideoTracks()[0];
            var trackSettings = track.getSettings && track.getSettings();
            this.videoDeviceId_ = trackSettings && trackSettings.deviceId ||
                null;
          }
          this.updateMirroring_();
          onSuccess();
        }.bind(this),
        function() {
          // TODO(mtomasz): Workaround for crbug.com/383241.
          setTimeout(tryStartWithConstraints.bind(this, index + 1), 0);
        });
  }.bind(this);

  this.videoDeviceIds_.then(function(deviceIds) {
    if (deviceIds.length == 0) {
      return Promise.reject("Device list empty.");
    }

    // Put the preferred camera first.
    var sortedDeviceIds = deviceIds.slice(0).sort(function(a, b) {
      if (a == b)
        return 0;
      if (a == this.videoDeviceId_)
        return -1;
      else
        return 1;
    }.bind(this));

    // Prepended 'null' deviceId means the system default camera. Add it only
    // when the app is launched (no deviceId_ set).
    if (this.videoDeviceId_ == null) {
        sortedDeviceIds.unshift(null);
    }
    sortedDeviceIds.forEach(function(deviceId) {
      var videoConstraints;
      if (this.is_recording_mode_) {
        // Video constraints for video recording are ordered by priority.
        videoConstraints = [
            {
              aspectRatio: { ideal: 1.7777777778 },
              width: { min: 1280 },
              frameRate: { min: 24 }
            },
            {
              width: { min: 640 },
              frameRate: { min: 24 }
            }];
      } else {
        // Video constraints for photo taking are ordered by priority.
        videoConstraints = [
            {
              aspectRatio: { ideal: 1.3333333333 },
              width: { min: 1280 },
              frameRate: { min: 24 }
            },
            {
              width: { min: 640 },
              frameRate: { min: 24 }
            }];
      }
      constraintsCandidates = videoConstraints.map(function(videoConstraint) {
        // Each passed-in video-constraint will be modified here.
        if (deviceId) {
          videoConstraint.deviceId = { exact: deviceId };
        } else {
          // As a default camera use the one which is facing the user.
          videoConstraint.facingMode = { exact: 'user' };
        }
        return { audio: this.is_recording_mode_, video: videoConstraint };
      }.bind(this));
    }.bind(this));

    tryStartWithConstraints(0);
  }.bind(this)).catch(function(error) {
    console.error('Failed to initialize camera.', error);
    onFailure();
  });
};

/**
 * Draws a single frame for the main canvas.
 * @param {camera.views.Camera.DrawMode} mode Drawing mode.
 * @private
 */
camera.views.Camera.prototype.drawCameraFrame_ = function(mode) {
  {
    var finishMeasuring = this.performanceMonitors_.startMeasuring(
        'main-fast-processor-load-contents-and-process');
    if (this.frame_ % 10 == 0 || mode == camera.views.Camera.DrawMode.FAST) {
      this.mainFastCanvasTexture_.loadContentsOf(this.video_);
      this.mainFastProcessor_.processFrame();
    }
    finishMeasuring();
  }

  switch (mode) {
    case camera.views.Camera.DrawMode.FAST:
      this.mainCanvas_.parentNode.hidden = true;
      this.mainPreviewCanvas_.parentNode.hidden = true;
      this.mainFastCanvas_.parentNode.hidden = false;
      break;
    case camera.views.Camera.DrawMode.NORMAL:
      {
        var finishMeasuring = this.performanceMonitors_.startMeasuring(
            'main-preview-processor-load-contents-and-process');
        this.mainPreviewCanvasTexture_.loadContentsOf(this.video_);
        this.mainPreviewProcessor_.processFrame();
        finishMeasuring();
      }
      this.mainCanvas_.parentNode.hidden = true;
      this.mainPreviewCanvas_.parentNode.hidden = false;
      this.mainFastCanvas_.parentNode.hidden = true;
      break;
    case camera.views.Camera.DrawMode.BEST:
      {
        var finishMeasuring = this.performanceMonitors_.startMeasuring(
            'main-processor-canvas-to-texture');
        this.mainCanvasTexture_.loadContentsOf(this.video_);
        finishMeasuring();
      }
      this.mainProcessor_.processFrame();
      {
        var finishMeasuring = this.performanceMonitors_.startMeasuring(
            'main-processor-dom');
        this.mainCanvas_.parentNode.hidden = false;
        this.mainPreviewCanvas_.parentNode.hidden = true;
        this.mainFastCanvas_.parentNode.hidden = true;
        finishMeasuring();
      }
      break;
  }
};

/**
 * Prints performance stats for named monitors to the console.
 * @private
 */
camera.views.Camera.prototype.printPerformanceStats_ = function() {
  console.info('Camera view');
  console.info(this.performanceMonitors_.toDebugString());
  console.info('Main processor');
  console.info(this.mainProcessor_.performanceMonitors.toDebugString());
  console.info('Main preview processor');
  console.info(this.mainPreviewProcessor_.performanceMonitors.toDebugString());
  console.info('Main fast processor');
  console.info(this.mainFastProcessor_.performanceMonitors.toDebugString());
};

/**
 * Handles the animation frame event and refreshes the viewport if necessary.
 * @private
 */
camera.views.Camera.prototype.onAnimationFrame_ = function() {
  // No capturing when the view is inactive.
  if (!this.active)
    return;

  // No capturing while resizing.
  if (this.context.resizing)
    return;

  // If the animation is called more often than the video provides input, then
  // there is no reason to process it. This will cup FPS to the Web Cam frame
  // rate (eg. head tracker interpolation, nor ghost effect will not be updated
  // more often than frames provided). Since we can assume that the webcam
  // serves frames with 30 FPS speed it should be OK. As a result, we will
  // significantly reduce CPU usage.
  if (this.lastFrameTime_ == this.video_.currentTime)
    return;

  var finishFrameMeasuring = this.performanceMonitors_.startMeasuring('main');
  this.frame_++;

  // Draw the camera frame. Decrease the rendering resolution when scrolling, or
  // while performing animations.
  {
    var finishMeasuring =
        this.performanceMonitors_.startMeasuring('draw-frame');
    if (this.toolbarEffect_.animating ||
        this.context.isUIAnimating() || this.toastEffect_.animating ||
        (this.scrollTracker_.scrolling && this.expanded_)) {
      this.drawCameraFrame_(camera.views.Camera.DrawMode.FAST);
    } else {
      this.drawCameraFrame_(camera.views.Camera.DrawMode.NORMAL);
    }
    finishMeasuring();
  }

  this.frame_++;
  finishFrameMeasuring();
  this.lastFrameTime_ = this.video_.currentTime;
};

/**
 * @override
 */
camera.views.Camera.prototype.onPictureDeleting = function(picture) {
  // No-op here (right before deleting a picure) as no picture would be deleted
  // when camera view is active and onActivate() already handles updating the
  // album-button.
};

/**
 * @override
 */
camera.views.Camera.prototype.onPictureAdded = function(picture) {
  this.updateAlbumButton_();
};

