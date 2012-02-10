// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MediaControls class implements media playback controls
 * that exist outside of the audio/video HTML element.
 */

/**
 * @param {HTMLElement} containerElement The container for the controls.
 * @param {function} onMediaError Function to display an error message.
 * @constructor
 */
function MediaControls(containerElement, onMediaError) {
  this.container_ = containerElement;
  this.document_ = this.container_.ownerDocument;
  this.media_ = null;

  this.onMediaPlayBound_ = this.onMediaPlay_.bind(this, true);
  this.onMediaPauseBound_ = this.onMediaPlay_.bind(this, false);
  this.onMediaDurationBound_ = this.onMediaDuration_.bind(this);
  this.onMediaProgressBound_ = this.onMediaProgress_.bind(this);
  this.onMediaError_ = onMediaError || function(){};
}

MediaControls.prototype.getMedia = function() { return this.media_ };

/**
 * Format the time in hh:mm:ss format (omitting redundant leading zeros).
 *
 * @param {number} timeInSec Time in seconds.
 * @return {string} Formatted time string.
 */
MediaControls.formatTime_ = function(timeInSec) {
  var seconds = Math.floor(timeInSec % 60);
  var minutes = Math.floor((timeInSec / 60) % 60);
  var hours = Math.floor(timeInSec / 60 / 60);
  var result = '';
  if (hours) result += hours + ':';
  if (hours && (minutes < 10)) result += '0';
  result += minutes + ':';
  if (seconds < 10) result += '0';
  result += seconds;
  return result;
};

/**
 * Create a custom control.
 *
 * @param {string} className
 * @param {HTMLElement=} opt_parent Parent element or container if undefined.
 * @return {HTMLElement}
 */
MediaControls.prototype.createControl = function(className, opt_parent) {
  opt_parent = opt_parent || this.container_;
  var control = this.document_.createElement('div');
  control.classList.add(className);
  opt_parent.appendChild(control);
  return control;
};

/**
 * Create a custom button.
 *
 * @param {string} className
 * @param {function(Event)} handler
 * @param {HTMLElement=} opt_parent Parent element or container if undefined.
 * @return {HTMLElement}
 */
MediaControls.prototype.createButton = function(
    className, handler, opt_parent) {
  var button = this.createControl(className, opt_parent);
  button.classList.add('media-button');
  button.addEventListener('click', handler);
  return button;
};

/*
 * Playback control.
 */

MediaControls.prototype.play = function() {
  this.media_.play();
};

MediaControls.prototype.pause = function() {
  this.media_.pause();
};

MediaControls.prototype.isPlaying = function() {
  return !this.media_.paused && !this.media_.ended;
};

MediaControls.prototype.togglePlayState = function() {
  if (this.isPlaying()) {
    this.media_.pause();
  } else {
    this.media_.play();
  }
};

MediaControls.prototype.createPlayButton = function(opt_parent) {
  this.playButton_ =
    this.createButton('play', this.togglePlayState.bind(this), opt_parent);
};

/*
 * Time controls
 */

// The default range of 100 is too coarse for the media progress slider.
// 1000 should be enough as the entire media controls area is never longer
// than 800px.
MediaControls.PROGRESS_RANGE = 1000;

MediaControls.prototype.createTimeControls = function(opt_parent) {
  var timeControls = this.createControl('time-controls', opt_parent);

  this.progressSlider_ = new MediaControls.PreciseSlider(
      this.createControl('progress', timeControls),
      0, /* value */
      MediaControls.PROGRESS_RANGE,
      this.onProgressChange_.bind(this),
      this.onProgressDrag_.bind(this));

  var timeBox = this.createControl('time', timeControls);

  this.duration_ = this.createControl('duration', timeBox);
  // Set the initial width to the minimum to reduce the flicker.
  this.duration_.textContent = MediaControls.formatTime_(0);

  this.currentTime_ = this.createControl('current', timeBox);
};

MediaControls.prototype.displayProgress_ = function(current, duration) {
  var ratio = current / duration;
  this.progressSlider_.setFilled(ratio);
  this.progressSlider_.setValue(ratio);
  this.currentTime_.textContent = MediaControls.formatTime_(current);
};

MediaControls.prototype.onProgressChange_ = function (value) {
  if (!this.media_.seekable || !this.media_.duration) {
    console.error("Inconsistent media state");
    return;
  }

  var current = this.media_.duration * value;
  this.media_.currentTime = current;
  this.currentTime_.textContent = MediaControls.formatTime_(current);
};

MediaControls.prototype.onProgressDrag_ = function (on) {
  if (on) {
    this.resumeAfterDrag_ = this.isPlaying();
    this.media_.pause();
  } else {
    if (this.resumeAfterDrag_){
      if (this.media_.ended)
        this.onMediaPlay_(false);
      else
        this.media_.play();
    }
  }
};

/*
 * Volume controls
 */

MediaControls.prototype.createVolumeControls = function(opt_parent) {
  var volumeControls = this.createControl('volume-controls', opt_parent);

  this.soundButton_ = this.createButton(
      'sound', this.onSoundButtonClick_.bind(this), volumeControls);

  this.volume_ = new MediaControls.Slider(
      this.createControl('volume', volumeControls),
      1, /* value */
      100 /* range */,
      this.onVolumeChange_.bind(this),
      this.onVolumeDrag_.bind(this));
};

MediaControls.prototype.onSoundButtonClick_ = function() {
  if (this.media_.volume == 0) {
    this.volume_.setValue(this.savedVolume_ || 1);
  } else {
    this.savedVolume_ = this.media_.volume;
    this.volume_.setValue(0);
  }
  this.onVolumeChange_(this.volume_.getValue());
};

MediaControls.getVolumeLevel_ = function (value) {
  if (value == 0) return 0;
  if (value <= 1/3) return 1;
  if (value <= 2/3) return 2;
  return 3;
};

MediaControls.prototype.onVolumeChange_ = function (value) {
  this.media_.volume = value;
  this.volume_.setFilled(value);
  this.soundButton_.setAttribute('level', MediaControls.getVolumeLevel_(value));
};

MediaControls.prototype.onVolumeDrag_ = function (on) {
  if (on && (this.media_.volume != 0)) {
    this.savedVolume_ = this.media_.volume;
  }
};

/*
 * Media event handlers.
 */

/**
 * Attach a media element.
 *
 * @param {HTMLMediaElement} mediaElement The media element to control.
 */
MediaControls.prototype.attachMedia = function(mediaElement) {
  this.media_ = mediaElement;

  this.media_.addEventListener('play', this.onMediaPlayBound_);
  this.media_.addEventListener('pause', this.onMediaPauseBound_);
  this.media_.addEventListener('durationchange', this.onMediaDurationBound_);
  this.media_.addEventListener('timeupdate', this.onMediaProgressBound_);
  this.media_.addEventListener('error', this.onMediaError_);

  // Reset the UI.
  this.playButton_.classList.remove('playing');
  this.displayProgress_(0, 1);
  /* Copy the volume from the UI to the media element. */
  this.onVolumeChange_(this.volume_.getValue());

  this.container_.classList.add('disabled');
};

/**
 * Detach media event handlers.
 */
MediaControls.prototype.detachMedia = function() {
  if (!this.media_)
    return;

  this.media_.removeEventListener('play', this.onMediaPlayBound_);
  this.media_.removeEventListener('pause', this.onMediaPauseBound_);
  this.media_.removeEventListener('durationchange', this.onMediaDurationBound_);
  this.media_.removeEventListener('timeupdate', this.onMediaProgressBound_);
  this.media_.removeEventListener('error', this.onMediaError_);

  this.media_ = null;
};

MediaControls.prototype.onMediaPlay_ = function(playing) {
  if (this.progressSlider_.isDragging())
    return;

  if (playing)
    this.playButton_.classList.add('playing');
  else
    this.playButton_.classList.remove('playing');
};

MediaControls.prototype.onMediaDuration_ = function() {
  if (!this.media_.duration)
    return;

  this.container_.classList.remove('disabled');

  if (this.media_.seekable) {
    this.progressSlider_.getContainer().classList.remove('readonly');
  } else {
    this.progressSlider_.getContainer().classList.add('readonly');
  }

  var valueToString = function(value) {
    return MediaControls.formatTime_(this.media_.duration * value);
  }.bind(this);

  this.duration_.textContent = valueToString(1);

  this.progressSlider_.setValueToStringFunction(valueToString);
};

MediaControls.prototype.onMediaProgress_ = function(e) {
  if (!this.media_.duration)
    return;

  var current = this.media_.currentTime;
  var duration = this.media_.duration;

  if (this.progressSlider_.isDragging()) {
    this.progressSlider_.setFilled(current / duration);
    return;
  }

  this.displayProgress_(current, duration);

  if (current == duration) {
    this.onMediaComplete();
  }
};

MediaControls.prototype.onMediaComplete = function() {};

/**
 * Create a customized slider control.
 *
 * @param {HTMLElement} container The containing div element.
 * @param {number} value Initial value [0..1].
 * @param {number} range Number of distinct slider positions to be supported.
 * @param {function(number)} onChange
 * @param {function(boolean)} onDrag
 * @constructor
 */

MediaControls.Slider = function(container, value, range, onChange, onDrag) {
  this.container_ =  container;
  this.onChange_ = onChange;
  this.onDrag_ = onDrag;

  var document = this.container_.ownerDocument;

  this.container_.classList.add('custom-slider');

  this.input_ = document.createElement('input');
  this.input_.type = 'range';
  this.input_.min = 0;
  this.input_.max = range;
  this.input_.value = value * range;
  this.container_.appendChild(this.input_);

  this.input_.addEventListener(
      'change', this.onInputChange_.bind(this));
  this.input_.addEventListener(
      'mousedown', this.onInputDrag_.bind(this, true));
  this.input_.addEventListener(
      'mouseup', this.onInputDrag_.bind(this, false));

  this.bar_ =  document.createElement('div');
  this.bar_.className = 'bar';
  this.container_.appendChild(this.bar_);

  this.filled_ =  document.createElement('div');
  this.filled_.className = 'filled';
  this.bar_.appendChild(this.filled_);

  var leftCap =  document.createElement('div');
  leftCap.className = 'cap left';
  this.bar_.appendChild(leftCap);

  var rightCap =  document.createElement('div');
  rightCap.className = 'cap right';
  this.bar_.appendChild(rightCap);
};

/**
 * @return {HTMLElement} The container element.
 */
MediaControls.Slider.prototype.getContainer = function() {
  return this.container_;
};

/**
 * @return {HTMLElement} The standard input element.
 */
MediaControls.Slider.prototype.getInput_ = function() {
  return this.input_;
};

/**
 * @return {HTMLElement} The slider bar element.
 */
MediaControls.Slider.prototype.getBar = function() {
  return this.bar_;
};

/**
 * @return {number} [0..1] The current value.
 */
MediaControls.Slider.prototype.getValue = function() {
  return this.input_.value / this.input_.max;
};

/**
 * @param {number} value [0..1]
 */
MediaControls.Slider.prototype.setValue = function(value) {
  this.input_.value = value * this.input_.max;
};

/**
 * Fill the given proportion the slider bar (from the left).
 *
 * @param {number} proportion [0..1]
 */
MediaControls.Slider.prototype.setFilled = function(proportion) {
  this.filled_.style.width = proportion * 100 + '%';
};

/**
 * Compute the proportion in which the given position divides the slider bar.
 *
 * @param {number} position in pixels.
 * @return {number} [0..1] proportion.
 */
MediaControls.Slider.prototype.getProportion = function (position) {
  var rect = this.bar_.getBoundingClientRect();
  return Math.max(0, Math.min(1, (position - rect.left) / rect.width));
};

MediaControls.Slider.prototype.onInputChange_ = function () {
  this.onChange_(this.getValue());
};

MediaControls.Slider.prototype.isDragging = function () {
  return this.isDragging_;
};

MediaControls.Slider.prototype.onInputDrag_ = function (on, event) {
  this.isDragging_ = on;
  this.onDrag_(on);
};

/**
 * Create a customized slider with a precise time feedback.
 *
 * The time value is shown above the slider bar at the mouse position.
 *
 * @param {HTMLElement} container The containing div element.
 * @param {number} value Initial value [0..1].
 * @param {number} range Number of distinct slider positions to be supported.
 * @param {function(number)} onChange
 * @param {function(boolean)} onDrag
 */
MediaControls.PreciseSlider = function(
    container, value, range, onChange, onDrag, formatFunction) {
  MediaControls.Slider.apply(this, arguments);

  var doc = this.container_.ownerDocument;

  /**
   * @type {function(number):string}
   */
  this.valueToString_ = null;

  this.seekMark_ =  doc.createElement('div');
  this.seekMark_.className = 'seek-mark';
  this.getBar().appendChild(this.seekMark_);

  this.seekLabel_ =  doc.createElement('div');
  this.seekLabel_.className = 'seek-label';
  this.seekMark_.appendChild(this.seekLabel_);

  this.getContainer().addEventListener(
      'mousemove', this.onMouseMove_.bind(this));
  this.getContainer().addEventListener(
      'mouseout', this.onMouseOut_.bind(this));
};

MediaControls.PreciseSlider.prototype = {
  __proto__: MediaControls.Slider.prototype
};

MediaControls.PreciseSlider.SHOW_DELAY = 200;
MediaControls.PreciseSlider.HIDE_AFTER_MOVE_DELAY = 2500;
MediaControls.PreciseSlider.HIDE_AFTER_DRAG_DELAY = 750;
MediaControls.PreciseSlider.NO_AUTO_HIDE = 0;

MediaControls.PreciseSlider.prototype.setValueToStringFunction =
    function(func) {
  this.valueToString_ = func;

  /* It is not completely accurate to assume that the max value corresponds
   to the longest string, but generous CSS padding will compensate for that. */
  var labelWidth = this.valueToString_(1).length / 2;
  this.seekLabel_.style.width = labelWidth + 'em';
  this.seekLabel_.style.marginLeft = -labelWidth/2 + 'em';
};

/**
 * Show the time above the slider.
 *
 * @param {number} ratio [0..1] The proportion of the duration.
 * @param {number} timeout Timeout in ms after which the label should be hidden.
 *   MediaControls.PreciseSlider.NO_AUTO_HIDE means show until the next call.
 */
MediaControls.PreciseSlider.prototype.showSeekMark_ =
    function (ratio, timeout) {
  // Do not update the seek mark for the first 500ms after the drag is finished.
  if (this.latestMouseUpTime_ && (this.latestMouseUpTime_ + 500 > Date.now()))
    return;

  this.seekMark_.style.left = ratio * 100 + '%';

  if (ratio < this.getValue()) {
    this.seekMark_.classList.remove('inverted');
  } else {
    this.seekMark_.classList.add('inverted');
  }
  this.seekLabel_.textContent = this.valueToString_(ratio);

  this.seekMark_.classList.add('visible');

  if (this.seekMarkTimer_) {
    clearTimeout(this.seekMarkTimer_);
    this.seekMarkTimer_ = null;
  }
  if (timeout != MediaControls.PreciseSlider.NO_AUTO_HIDE) {
    this.seekMarkTimer_ = setTimeout(this.hideSeekMark_.bind(this), timeout);
  }
};

MediaControls.PreciseSlider.prototype.hideSeekMark_ = function () {
  this.seekMarkTimer_ = null;
  this.seekMark_.classList.remove('visible');
};

MediaControls.PreciseSlider.prototype.onMouseMove_ = function (event) {
  this.latestSeekRatio_ = this.getProportion(event.clientX);

  var self = this;
  function showMark() {
    if (!self.isDragging()) {
      self.showSeekMark_(self.latestSeekRatio_,
          MediaControls.PreciseSlider.HIDE_AFTER_MOVE_DELAY);
    }
  }

  if (this.seekMark_.classList.contains('visible')) {
    showMark();
  } else if (!this.seekMarkTimer_) {
    this.seekMarkTimer_ =
        setTimeout(showMark, MediaControls.PreciseSlider.SHOW_DELAY);
  }
};

MediaControls.PreciseSlider.prototype.onMouseOut_ = function (e) {
  for (var element = e.relatedTarget; element; element = element.parentNode) {
    if (element == this.getContainer()) {
      return;
    }
  }
  if (this.seekMarkTimer_) {
    clearTimeout(this.seekMarkTimer_);
    this.seekMarkTimer_ = null;
  }
  this.hideSeekMark_();
};

MediaControls.PreciseSlider.prototype.onInputChange_ = function () {
  MediaControls.Slider.prototype.onInputChange_.apply(this, arguments);
  if (this.isDragging()) {
    this.showSeekMark_(
        this.getValue(), MediaControls.PreciseSlider.NO_AUTO_HIDE);
  }
};

MediaControls.PreciseSlider.prototype.onInputDrag_ = function (on, event) {
  MediaControls.Slider.prototype.onInputDrag_.apply(this, arguments);

  if (on) {
    // Dragging started, align the seek mark with the thumb position.
    this.showSeekMark_(
        this.getValue(), MediaControls.PreciseSlider.NO_AUTO_HIDE);
  } else {
    // Just finished dragging.
    // Show the label for the last time with a shorter timeout.
    this.showSeekMark_(
        this.getValue(), MediaControls.PreciseSlider.HIDE_AFTER_DRAG_DELAY);
    this.latestMouseUpTime_ = Date.now();
  }
};

/**
 * Create video controls.
 *
 * @param {HTMLElement} containerElement The container for the controls.
 * @param {function} onMediaError Function to display an error message.
 * @param {function} opt_fullScreenToggle Function to toggle fullscreen mode.
 * @constructor
 */
function VideoControls(containerElement, onMediaError, opt_fullScreenToggle) {
  MediaControls.call(this, containerElement, onMediaError);

  this.container_.classList.add('video-controls');

  this.createPlayButton();

  this.createTimeControls();

  this.createVolumeControls();

  if (opt_fullScreenToggle) {
    this.fullscreenButton_ =
        this.createButton('fullscreen', opt_fullScreenToggle);
  }
}

VideoControls.prototype = { __proto__: MediaControls.prototype };

VideoControls.prototype.onMediaComplete = function() {
  this.onMediaPlay_(false);  // Just update the UI.
};
