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
 * @constructor
 */
camera.views.Camera = function(context, router) {
  camera.View.call(this, context, router);

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
   * Canvas element with the current frame downsampled to small resolution, to
   * be used in effect preview windows.
   *
   * @type {Canvas}
   * @private
   */
  this.previewInputCanvas_ = document.createElement('canvas');

  /**
   * @type {boolean}
   * @private
   */
  this.running_ = false;

  /**
   * @type {boolean}
   * @private
   */
  this.capturing_ = false;

  /**
   * The main (full screen) canvas for full quality capture.
   * @type {fx.Canvas}
   * @private
   */
  this.mainCanvas_ = null;

  /**
   * The main (full screen canvas) for fast capture.
   * @type {fx.Canvas}
   * @private
   */
  this.mainFastCanvas_ = null;

  /**
   * Shared fx canvas for effects' previews.
   * @type {fx.Canvas}
   * @private
   */
  this.previewCanvas_ = null;

  /**
   * The main (full screen) processor in the full quality mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainProcessor_ = null;

  /**
   * The main (full screen) processor in the fast mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainFastProcessor_ = null;

  /**
   * Processors for effect previews.
   * @type {Array.<camera.Processor>}
   * @private
   */
  this.previewProcessors_ = [];

  /**
   * Selected effect or null if no effect.
   * @type {number}
   * @private
   */
  this.currentEffectIndex_ = 0;

  /**
   * Face detector and tracker.
   * @type {camera.Tracker}
   * @private
   */
  this.tracker_ = new camera.Tracker(this.previewInputCanvas_);

  /**
   * Current frame.
   * @type {number}
   * @private
   */
  this.frame_ = 0;

  /**
   * If the tools are expanded.
   * @type {boolean}
   * @private
   */
  this.expanded_ = false;

  /**
   * Tools animation effect wrapper.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.toolsEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        if (args)
          document.body.classList.add('expanded');
        else
          document.body.classList.remove('expanded');
        camera.util.waitForTransitionCompletion(
            document.querySelector('#toolbar'), 500, callback);
      });

  /**
   * Whether a picture is being taken. Used to decrease video quality of
   * previews for smoother response.
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
   * True if the window has been shown during startup. Used to avoid showing
   * it more than once, when retrying initialization.
   * @type {boolean}
   * @private
   */
  this.shownAtStartup_ = false;

  /**
   * Detects if the mouse has been moved or clicked, and if any touch events
   * have been performed on the view. If such events are detected, then the
   * ribbon and the window buttons are shown.
   *
   * @type {camera.util.PointerTracker}
   * @private
   */
  this.pointerTracker_ = new camera.util.PointerTracker(
      document.body, this.setExpanded_.bind(this, true));

  // End of properties, seal the object.
  Object.seal(this);

  // Synchronize bounds of the video now, when window is resized or if the
  // video dimensions are loaded.
  this.video_.addEventListener('loadedmetadata',
      this.synchronizeBounds_.bind(this));
  this.synchronizeBounds_();

  // TODO(mtomasz): Make the ribbon scrollable by dragging.

  // Handle the 'Take' button.
  document.querySelector('#take-picture').addEventListener(
      'click', this.takePicture_.bind(this));

  // Load the shutter sound.
  this.shutterSound_.src = '../sounds/shutter.ogg';
};

/**
 * Frame draw mode.
 * @enum {number}
 */
camera.views.Camera.DrawMode = Object.freeze({
  OPTIMIZED: 0,  // Quality optimized for performance.
  FULL: 1  // The best quality possible.
});

/**
 * Number of frames to be skipped between optimized effects' ribbon refreshes
 * and the head detection invocations (which both use the preview back buffer).
 *
 * @type {number}
 * @const
 */
camera.views.Camera.PREVIEW_BUFFER_SKIP_FRAMES = 3;

/**
 * Number of frames to be skipped between effects' ribbon full refreshes.
 * @type {number}
 * @const
 */
camera.views.Camera.EFFECTS_RIBBON_FULL_REFRESH_SKIP_FRAMES = 30;

camera.views.Camera.prototype = {
  __proto__: camera.View.prototype,
  get running() {
    return this.running_;
  },
  get capturing() {
    return this.capturing_;
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
    this.mainFastCanvas_ = fx.canvas();
    this.previewCanvas_ = fx.canvas();
  }
  catch (e) {
    // TODO(mtomasz): Replace with a better icon.
    this.context_.onError('no-camera',
        chrome.i18n.getMessage('errorMsgNoWebGL'),
        chrome.i18n.getMessage('errorMsgNoWebGLHint'));

    // Show the window, since the camera initialization will not happen (due
    // to lack of webgl).
    chrome.app.window.current().show();
    this.shownAtStartup_ = true;
  }

  if (this.mainCanvas_ && this.mainFastCanvas_) {
    // Initialize the processors.
    this.mainProcessor_ = new camera.Processor(
        this.video_, this.mainCanvas_, this.mainCanvas_);
    this.mainFastProcessor_ = new camera.Processor(
        this.video_,
        this.mainFastCanvas_,
        this.mainFastCanvas_,
        camera.Processor.Mode.FAST);

    // Insert the main canvas to its container.
    document.querySelector('#main-canvas-wrapper').appendChild(
        this.mainCanvas_);
    document.querySelector('#main-fast-canvas-wrapper').appendChild(
        this.mainFastCanvas_);

    // Set the default effect.
    this.mainProcessor_.effect = new camera.effects.Swirl();

    // Prepare effect previews.
    this.addEffect_(new camera.effects.Normal(this.tracker_));
    this.addEffect_(new camera.effects.Vintage(this.tracker_));
    this.addEffect_(new camera.effects.Beauty(this.tracker_));
    this.addEffect_(new camera.effects.BigHead(this.tracker_));
    this.addEffect_(new camera.effects.BigJaw(this.tracker_));
    this.addEffect_(new camera.effects.BigEyes(this.tracker_));
    this.addEffect_(new camera.effects.BunnyHead(this.tracker_));
    this.addEffect_(new camera.effects.Swirl(this.tracker_));
    this.addEffect_(new camera.effects.Grayscale(this.tracker_));
    this.addEffect_(new camera.effects.Sepia(this.tracker_));
    this.addEffect_(new camera.effects.Colorize(this.tracker_));
    this.addEffect_(new camera.effects.Newspaper(this.tracker_));
    this.addEffect_(new camera.effects.Funky(this.tracker_));
    this.addEffect_(new camera.effects.TiltShift(this.tracker_));
    this.addEffect_(new camera.effects.Cinema(this.tracker_));

    // Select the default effect.
    this.setCurrentEffect_(0);
  }

  // Acquire the gallery model.
  camera.models.Gallery.getInstance(function(model) {
    this.model_ = model;
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
  if (!this.running_ && this.mainCanvas_ && this.mainFastCanvas_)
    this.start_();

  this.onResize();
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Camera.prototype.onLeave = function() {
};


/**
 * Adds an effect to the user interface.
 * @param {camera.Effect} effect Effect to be added.
 * @private
 */
camera.views.Camera.prototype.addEffect_ = function(effect) {
  // Create the preview on the ribbon.
  var list = document.querySelector('#effects .padder');
  var wrapper = document.createElement('div');
  wrapper.className = 'preview-canvas-wrapper';
  var canvas = document.createElement('canvas');
  var padder = document.createElement('div');
  padder.className = 'preview-canvas-padder';
  padder.appendChild(canvas);
  wrapper.appendChild(padder);
  var item = document.createElement('li');
  item.appendChild(wrapper);
  list.appendChild(item);
  var label = document.createElement('span');
  label.textContent = effect.getTitle();
  item.appendChild(label);

  // Calculate the effect index.
  var effectIndex = this.previewProcessors_.length;
  item.id = 'effect-' + effectIndex;

  // Assign events.
  item.addEventListener('click',
      this.setCurrentEffect_.bind(this, effectIndex));

  // Create the preview processor.
  var processor = new camera.Processor(
      this.previewInputCanvas_, canvas, this.previewCanvas_);
  processor.effect = effect;
  this.previewProcessors_.push(processor);
};

/**
 * Sets the current effect.
 * @param {number} effectIndex Effect index.
 * @private
 */
camera.views.Camera.prototype.setCurrentEffect_ = function(effectIndex) {
  document.querySelector('#effects #effect-' + this.currentEffectIndex_).
      removeAttribute('selected');
  var element = document.querySelector('#effects #effect-' + effectIndex);
  element.setAttribute('selected', '');
  camera.util.ensureVisible(element, this.scroller_);
  if (this.currentEffectIndex_ == effectIndex)
    this.previewProcessors_[effectIndex].effect.randomize();
  this.mainProcessor_.effect = this.previewProcessors_[effectIndex].effect;
  this.mainFastProcessor_.effect = this.previewProcessors_[effectIndex].effect;
  this.currentEffectIndex_ = effectIndex;
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
  switch (event.keyIdentifier) {
    case 'Left':
      this.setCurrentEffect_(
          (this.currentEffectIndex_ + this.previewProcessors_.length - 1) %
              this.previewProcessors_.length);
      event.preventDefault();
      break;
    case 'Right':
      this.setCurrentEffect_(
          (this.currentEffectIndex_ + 1) % this.previewProcessors_.length);
      event.preventDefault();
      break;
    case 'Home':
      this.setCurrentEffect_(0);
      event.preventDefault();
      break;
    case 'End':
      this.setCurrentEffect_(this.previewProcessors_.length - 1);
      event.preventDefault();
      break;
    case 'Enter':
    case 'U+0020':
      this.takePicture_();
      event.stopPropagation();
      event.preventDefault();
      break;
  }
};

/**
 * @override
 */
camera.views.Camera.prototype.onMouseWheel = function(event) {
  // TODO(mtomasz): Introduce an effects model.
  if (event.wheelDelta > 0) {
    this.setCurrentEffect_(
        (this.currentEffectIndex_ + this.previewProcessors_.length - 1) %
            this.previewProcessors_.length);
  } else {
    this.setCurrentEffect_(
        (this.currentEffectIndex_ + 1) % this.previewProcessors_.length);
    event.preventDefault();
  }
};

/**
 * Handles scrolling via mouse on the effects ribbon.
 * @param {Event} event Mouse move event.
 * @private
 */
camera.views.Camera.prototype.onRibbonMouseMove_ = function(event) {
  if (event.which != 1)
    return;
  var ribbon = document.querySelector('#effects');
  ribbon.scrollLeft = ribbon.scrollLeft - event.webkitMovementX;
};

/**
 * Toggles the tools visibility.
 * @param {boolean} expanded True to show the tools, false to hide.
 * @private
 */
camera.views.Camera.prototype.setExpanded_ = function(expanded) {
  if (this.collapseTimer_) {
    clearTimeout(this.collapseTimer_);
    this.collapseTimer_ = null;
  }
  if (expanded) {
    this.collapseTimer_ = setTimeout(this.setExpanded_.bind(this, false), 2000);
    if (!this.expanded_) {
      this.toolsEffect_.invoke(true, function() {
        this.expanded_ = true;
      }.bind(this));
    }
  } else {
    if (this.expanded_) {
      this.expanded_ = false;
      this.toolsEffect_.invoke(false, function() {});
    }
  }
};

/**
 * Chooses a file stream to override the camera stream. Used for debugging.
 */
camera.views.Camera.prototype.chooseFileStream = function() {
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
 * Takes the picture, saves and puts to the gallery with a nice animation.
 * @private
 */
camera.views.Camera.prototype.takePicture_ = function() {
  if (!this.running_)
    return;

  // Lock refreshing for smoother experience.
  this.taking_ = true;

  // Show flashing animation with the shutter sound.
  document.body.classList.add('show-shutter');
  var galleryButton = document.querySelector('#toolbar .gallery-switch');
  camera.util.setAnimationClass(galleryButton, galleryButton, 'flash');
  setTimeout(function() {
    document.body.classList.remove('show-shutter');
  }.bind(this), 200);

  this.shutterSound_.currentTime = 0;
  this.shutterSound_.play();

  setTimeout(function() {
    this.mainProcessor_.processFrame();
    var dataURL = this.mainCanvas_.toDataURL('image/jpeg');

    // Create a picture preview animation.
    var picturePreview = document.querySelector('#picture-preview');
    var img = document.createElement('img');
    img.src = dataURL;
    img.style.webkitTransform = 'rotate(' + (Math.random() * 40 - 20) + 'deg)';

    // Create the fly-away animation after two second.
    setTimeout(function() {
      var sourceRect = img.getBoundingClientRect();
      var targetRect = galleryButton.getBoundingClientRect();

      var translateXValue = (targetRect.left + targetRect.right) / 2 -
          (sourceRect.left + sourceRect.right) / 2;
      var translateYValue = (targetRect.top + targetRect.bottom) / 2 -
          (sourceRect.top + sourceRect.bottom) / 2;
      var scaleValue = targetRect.width / sourceRect.width;

      img.style.webkitTransform =
          'rotate(0) translateX(' + translateXValue +'px) ' +
          'translateY(' + translateYValue + 'px) ' +
          'scale(' + scaleValue + ')';
      img.style.opacity = 0;

      camera.util.waitForTransitionCompletion(img, 1500, function() {
        img.parentNode.removeChild(img);
        this.taking_ = false;
      }.bind(this));
    }.bind(this), 2000);

    // Add to DOM.
    picturePreview.appendChild(img);

    // Call the callback with the picture.
    this.model_.addPicture(dataURL);
  }.bind(this), 0);
};

/**
 * Resolutions to be probed on the camera. Format: [[width, height], ...].
 * @type {Array.<Array.<number>>}
 * @const
 */
camera.views.Camera.RESOLUTIONS = [[1280, 720], [800, 600], [640, 480]];

/**
 * Synchronizes video size with the window's current size.
 * @private
 */
camera.views.Camera.prototype.synchronizeBounds_ = function() {
  if (!this.video_.videoWidth)
    return;

  var width = document.body.offsetWidth;
  var height = document.body.offsetHeight;
  var windowRatio = width / height;
  var videoRatio = this.video_.videoWidth / this.video_.videoHeight;

  this.video_.width = this.video_.videoWidth;
  this.video_.height = this.video_.videoHeight;

  // Add 1 pixel to avoid artifacts.
  var zoom = (width + 1) / this.video_.videoWidth;

  // Set resolution of the low-resolution preview input canvas.
  if (videoRatio < 1.5) {
    // For resolutions: 800x600.
    this.previewInputCanvas_.width = 120;
    this.previewInputCanvas_.height = 90;
  } else {
    // For wide resolutions (any other).
    this.previewInputCanvas_.width = 192;
    this.previewInputCanvas_.height = 108;
  }
};

/**
 * Starts capturing with the specified resolution.
 *
 * @param {Array.<number>} resolution Width and height of the capturing mode,
 *     eg. [800, 600].
 * @param {function(number, number)} onSuccess Success callback with the set
 *     resolution.
 * @param {function()} onFailure Failure callback, eg. the resolution is
 *     not supported.
 * @param {function()} onDisconnected Called when the camera connection is lost.
 * @private
 */
 camera.views.Camera.prototype.startWithResolution_ =
     function(resolution, onSuccess, onFailure, onDisconnected) {
  if (this.running_)
    this.stop();

  navigator.webkitGetUserMedia({
    video: {
      mandatory: {
        minWidth: resolution[0],
        minHeight: resolution[1]
      }
    }
  }, function(stream) {
    this.running_ = true;
    // Use a watchdog since the stream.onended event is unreliable in the
    // recent version of Chrome.
    this.watchdog_ = setInterval(function() {
      if (stream.ended) {
        this.capturing_ = false;
        onDisconnected();
        clearTimeout(this.watchdog_);
        this.watchdog_ = null;
      }
    }.bind(this), 1000);
    this.video_.src = window.URL.createObjectURL(stream);
    this.video_.play();
    this.capturing_ = true;
    var onAnimationFrame = function() {
      if (!this.running_)
        return;
      this.onAnimationFrame_();
      requestAnimationFrame(onAnimationFrame);
    }.bind(this);
    onAnimationFrame();
    onSuccess(resolution[0], resolution[1]);
  }.bind(this), function(error) {
    onFailure();
  });
};

/**
 * Stops capturing the camera.
 */
camera.views.Camera.prototype.stop = function() {
  this.running_ = false;
  this.capturing_ = false;
  this.video_.pause();
  this.video_.src = '';
  if (this.watchdog_) {
    clearTimeout(this.watchdog_);
    this.watchdog_ = null;
  }
};

/**
 * Starts capturing the camera with the highest possible resolution.
 * @private
 */
camera.views.Camera.prototype.start_ = function() {
  var index = 0;

  var onSuccess = function(width, height) {
    // Set the default dimensions to at most half of the available width
    // and to the compatible aspect ratio. 640/360 dimensions are used to
    // detect that the window has never been opened.
    var windowWidth = document.body.offsetWidth;
    var windowHeight = document.body.offsetHeight;
    var targetAspectRatio = width / height;
    var targetWidth = Math.round(Math.min(width, screen.width * 0.8));
    var targetHeight = Math.round(targetWidth / targetAspectRatio);
    if (windowWidth == 640 && windowHeight == 360)
      chrome.app.window.current().resizeTo(targetWidth, targetHeight);
    if (!this.shownAtStartup_)
      chrome.app.window.current().show();
    // Show tools after some small delay to make it more visible.
    setTimeout(function() {
      this.ribbonInitialization_ = false;
      this.setExpanded_(true);
    }.bind(this), 500);
    if (this.retryStartTimer_) {
      clearTimeout(this.retryStartTimer_);
      this.retryStartTimer_ = null;
    }
    this.context_.onErrorRecovered('no-camera');
  }.bind(this);

  var scheduleRetry = function() {
    this.context_.onError(
        'no-camera',
        chrome.i18n.getMessage('errorMsgNoCamera'),
        chrome.i18n.getMessage('errorMsgNoCameraHint'));
    if (this.retryStartTimer_) {
      clearTimeout(this.retryStartTimer_);
      this.retryStartTimer_ = null;
    }
    this.retryStartTimer_ = setTimeout(this.start_.bind(this), 1000);
  }.bind(this);

  var onFailure = function() {
    if (!this.shownAtStartup_)
      chrome.app.window.current().show();
    this.shownAtStartup_ = true;
    scheduleRetry();
  }.bind(this);

  var tryNextResolution = function() {
    this.startWithResolution_(
        camera.views.Camera.RESOLUTIONS[index],
        onSuccess,
        function() {
          index++;
          if (index < camera.views.Camera.RESOLUTIONS.length)
            tryNextResolution();
          else
            onFailure();
        },
        scheduleRetry.bind(this));  // onDisconnected
  }.bind(this);

  tryNextResolution();
};

/**
 * Draws the effects' ribbon.
 * @param {camera.views.Camera.DrawMode} mode Drawing mode.
 * @private
 */
camera.views.Camera.prototype.drawEffectsRibbon_ = function(mode) {
  for (var index = 0; index < this.previewProcessors_.length; index++) {
    var processor = this.previewProcessors_[index];
    var effectRect = processor.output.getBoundingClientRect();
    switch (mode) {
      case camera.views.Camera.DrawMode.OPTIMIZED:
        if (effectRect.right >= 0 &&
            effectRect.left < document.body.offsetWidth) {
          processor.processFrame();
        }
        break;
      case camera.views.Camera.DrawMode.FULL:
        processor.processFrame();
        break;
    }
  }
 };

/**
 * Draws a single frame for the main canvas and effects.
 * @param {camera.views.Camera.DrawMode} mode Drawing mode.
 * @private
 */
camera.views.Camera.prototype.drawCameraFrame_ = function(mode) {
  switch (mode) {
    case camera.views.Camera.DrawMode.OPTIMIZED:
      this.mainFastProcessor_.processFrame();
      this.mainCanvas_.parentNode.hidden = true;
      this.mainFastCanvas_.parentNode.hidden = false;
      break;
    case camera.views.Camera.DrawMode.FULL:
      this.mainProcessor_.processFrame();
      this.mainCanvas_.parentNode.hidden = false;
      this.mainFastCanvas_.parentNode.hidden = true;
      break;
  }
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

  // Copy the video frame to the back buffer. The back buffer is low
  // resolution, since it is only used by the effects' previews as by the
  // head tracker.
  if (this.frame_ % camera.views.Camera.PREVIEW_BUFFER_SKIP_FRAMES == 0) {
    var context = this.previewInputCanvas_.getContext('2d');
    context.drawImage(this.video_,
                      0,
                      0,
                      this.previewInputCanvas_.width,
                      this.previewInputCanvas_.height);

    // Detect and track faces.
    this.tracker_.detect();
  }

  // Update internal state of the tracker.
  this.tracker_.update();

  // Draw the camera frame. Decrease the rendering resolution when scrolling, or
  // while performing animations.
  if (this.taking_ || this.toolsEffect_.isActive() ||
      this.mainProcessor_.effect.isSlow() ||
      (this.scroller_.animating && this.expanded_)) {
    this.drawCameraFrame_(camera.views.Camera.DrawMode.OPTIMIZED);
  } else {
    this.drawCameraFrame_(camera.views.Camera.DrawMode.FULL);
  }

  // Draw the effects' ribbon.
  // Process effect preview canvases. Ribbon initialization is true before the
  // ribbon is expanded for the first time. This trick is used to fill the
  // ribbon with images as soon as possible.
   if (this.expanded_ && !this.taking_ &&
      !this.scroller_.animating || this.ribbonInitialization_) {

     // Every third frame draw only the visible effects. Every 30-th frame, draw
     // all of them, to avoid displaying old effects when scrolling.
     if (this.frame_ %
         camera.views.Camera.EFFECTS_RIBBON_FULL_REFRESH_SKIP_FRAMES == 0) {
       this.drawEffectsRibbon_(camera.views.Camera.DrawMode.FULL);
     } else if (this.frame_ %
         camera.views.Camera.PREVIEW_BUFFER_SKIP_FRAMES == 0) {
       this.drawEffectsRibbon_(camera.views.Camera.DrawMode.OPTIMIZED);
     }
  }

  this.frame_++;
};

