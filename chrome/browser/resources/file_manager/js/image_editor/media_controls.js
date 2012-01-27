// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MediaControls class implements media playback controls
 * that exist outside of the audio/video HTML element.
 */

/**
 * @param {HTMLMediaElement} mediaElement The media element to control.
 * @param {HTMLElement} containerElement The container for the controls.
 * @constructor
 */
function MediaControls(mediaElement, containerElement) {
  this.media_ = mediaElement;
  this.container_ = containerElement;
  this.document_ = this.container_.ownerDocument;

  this.setupPlaybackControls_();

  this.setupMediaEvents_();
}

/**
 * Load the given url into the media element.
 *
 * @param {string} url
 */
MediaControls.prototype.load = function(url) {
  this.media_.src = url;
  this.media_.load();

  // Reset the UI as the load call above did not raise any media events.
  this.playButton_.classList.remove('playing');
  this.displayProgress_(0, 1);
};

MediaControls.prototype.play = function() {
  this.media_.play();
};

MediaControls.prototype.pause = function() {
  this.media_.pause();
};

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
 * @return {HTMLElement}
 */
MediaControls.prototype.createControl_ = function(className) {
  var control = this.document_.createElement('div');
  control.classList.add(className);
  this.container_.appendChild(control);
  return control;
};

/**
 * Create a custom button.
 *
 * @param {string} className
 * @param {function(Event)} handler
 * @return {HTMLElement}
 */
MediaControls.prototype.createButton_ = function(className, handler) {
  var button = this.createControl_(className);
  button.classList.add('media-button');
  button.addEventListener('click', handler);
  return button;
};

// The default range of 100 is too coarse for the media progress slider.
// 1000 should be enough as the entire media controls area is never longer
// than 800px.
MediaControls.PROGRESS_RANGE = 1000;

/**
 * Create playback controls in DOM.
 */
MediaControls.prototype.setupPlaybackControls_ = function() {
  this.playButton_ = this.createButton_(
      'play', this.onPlayButtonClick_.bind(this));

  this.progress_ = new MediaControls.Slider(
      this.createControl_('progress'),
      0, /* value */
      MediaControls.PROGRESS_RANGE);

  this.progress_.getContainer().addEventListener(
      'mousemove', this.onProgressMouseMove_.bind(this));
  this.progress_.getContainer().addEventListener(
      'mouseout', this.onProgressMouseOut_.bind(this));

  this.progress_.getInput_().addEventListener(
      'change', this.onProgressChange_.bind(this));
  this.progress_.getInput_().addEventListener(
      'mousedown', this.onProgressMouseDown_.bind(this));
  this.progress_.getInput_().addEventListener(
      'mouseup', this.onProgressMouseUp_.bind(this));

  this.seekMark_ =  this.document_.createElement('div');
  this.seekMark_.className = 'seek-mark';
  this.progress_.getBar().appendChild(this.seekMark_);

  this.seekLabel_ =  this.document_.createElement('div');
  this.seekLabel_.className = 'seek-label';
  this.seekMark_.appendChild(this.seekLabel_);

  this.currentTime_ = this.createControl_('current-time');
  this.currentTime_.textContent = MediaControls.formatTime_(0);

  this.soundButton_ = this.createButton_(
      'sound', this.onSoundButtonClick_.bind(this));

  this.volume_ = new MediaControls.Slider(
      this.createControl_('volume'),
      this.media_.volume,
      100 /* range */);
  this.onVolumeChange_();

  this.volume_.getInput_().addEventListener(
      'change', this.onVolumeChange_.bind(this));
  this.volume_.getInput_().addEventListener(
      'mousedown', this.onVolumeMouseDown_.bind(this));

  this.volume_.setValue(this.media_.volume);

  this.fullscreenButton_ = this.createButton_(
      'fullscreen', this.onFullscreenButtonClick_.bind(this));
};

MediaControls.prototype.displayProgress_ = function(current, duration) {
  var ratio = current / duration;
  this.progress_.setFilled(ratio);
  this.progress_.setValue(ratio);
  this.currentTime_.textContent = MediaControls.formatTime_(current);
};

MediaControls.prototype.onPlayButtonClick_ = function() {
  if (this.media_.paused || this.media_.ended) {
    this.media_.play();
  } else {
    this.media_.pause();
  }
};

MediaControls.prototype.onProgressChange_ = function () {
// TODO(kaznacheev): Handle !this.media_.seekable in a meaningful manner.
  if (this.media_.seekable && this.media_.duration) {
    var current = this.media_.duration * this.progress_.getValue();
    this.media_.currentTime = current;
    this.currentTime_.textContent = MediaControls.formatTime_(current);
  }
  if (this.progressDragStatus_) {
    this.showSeekMark_(this.progress_.getValue());
  }
};

MediaControls.prototype.onProgressMouseDown_ = function () {
  this.progressDragStatus_ = {
    paused: this.media_.paused
  };
  this.media_.pause();
  this.showSeekMark_(this.progress_.getValue());
};

MediaControls.prototype.onProgressMouseUp_ = function () {
  var dragStatus = this.progressDragStatus_;
  this.progressDragStatus_ = null;
  if (!dragStatus.paused) {
    if (this.media_.currentTime == this.media_.duration)
      this.onMediaComplete_();
    else
      this.media_.play();
  }
};

MediaControls.prototype.onProgressMouseMove_ = function (e) {
  this.latestSeekRatio_ = this.progress_.getProportion(e.clientX);

  var self = this;
  function showMark() {
    if (!self.progressDragStatus_) {
      self.showSeekMark_(self.latestSeekRatio_);
    }
  }

  if (this.seekMark_.classList.contains('visible')) {
    showMark();
  } else if (!this.seekMarkTimer_) {
    this.seekMarkTimer_ = setTimeout(showMark, 200);
  }
};

MediaControls.prototype.onProgressMouseOut_ = function (e) {
  for (var element = e.relatedTarget; element; element = element.parentNode) {
    if (element == this.progress_.getContainer()) {
      return;
    }
  }
  if (this.seekMarkTimer_) {
    clearTimeout(this.seekMarkTimer_);
    this.seekMarkTimer_ = null;
  }
  this.hideSeekMark_();
};

MediaControls.prototype.showSeekMark_ = function (ratio) {
  this.seekMark_.style.left = ratio * 100 + '%';

  if (ratio < this.media_.currentTime / this.media_.duration) {
    this.seekMark_.classList.remove('inverted');
  } else {
    this.seekMark_.classList.add('inverted');
  }
  this.seekLabel_.textContent =
      MediaControls.formatTime_(this.media_.duration * ratio);

  this.seekMark_.classList.add('visible');

  if (this.seekMarkTimer_) {
    clearTimeout(this.seekMarkTimer_);
    this.seekMarkTimer_ = null;
  }
  this.seekMarkTimer_ = setTimeout(this.hideSeekMark_.bind(this), 3000);
};

MediaControls.prototype.hideSeekMark_ = function () {
  this.seekMarkTimer_ = null;
  this.seekMark_.classList.remove('visible');
};

MediaControls.prototype.onSoundButtonClick_ = function() {
  if (this.media_.volume == 0) {
    this.volume_.setValue(this.savedVolume_ || 1);
  } else {
    this.savedVolume_ = this.media_.volume;
    this.volume_.setValue(0);
  }
  this.onVolumeChange_();
};

MediaControls.prototype.onVolumeChange_ = function () {
  var value = this.volume_.getValue();
  this.media_.volume = value;
  this.volume_.setFilled(value);
  if (value != 0) {
    this.soundButton_.classList.remove('muted');
  } else {
    this.soundButton_.classList.add('muted');
  }
};

MediaControls.prototype.onVolumeMouseDown_ = function () {
  if (this.media_.volume != 0) {
    this.savedVolume_ = this.media_.volume;
  }
};

MediaControls.prototype.onFullscreenButtonClick_ = function() {
  this.fullscreenButton_.classList.toggle('on');
  // TODO: invoke the actual full screen mode
};

/**
 * Attach media event handlers.
 */
MediaControls.prototype.setupMediaEvents_ = function() {
  this.media_.addEventListener(
      'play', this.onMediaPlay_.bind(this, true));
  this.media_.addEventListener(
      'pause', this.onMediaPlay_.bind(this, false));
  this.media_.addEventListener(
      'durationchange', this.onMediaDuration_.bind(this));
  this.media_.addEventListener(
      'timeupdate', this.onMediaProgress_.bind(this));
  this.media_.addEventListener(
      'error', this.onMediaError_.bind(this));
};

MediaControls.prototype.onMediaPlay_ = function(playing) {
  if (this.progressDragStatus_)
    return;

  if (playing)
    this.playButton_.classList.add('playing');
  else
    this.playButton_.classList.remove('playing');
};

MediaControls.prototype.onMediaDuration_ = function() {
  if (!this.media_.duration)
    return;

  var length = MediaControls.formatTime_(this.media_.duration).length;
  var width = (length + 1) / 2;
  this.currentTime_.style.width = width + 'em';
  this.seekLabel_.style.width = width + 'em';
  this.seekLabel_.style.marginLeft = -width/2 + 'em';
};

MediaControls.prototype.onMediaProgress_ = function(e) {
  if (!this.media_.duration)
    return;

  var current = this.media_.currentTime;
  var duration = this.media_.duration;

  if (this.progressDragStatus_) {
    this.progress_.setFilled(current / duration);
    return;
  }

  this.displayProgress_(current, duration);

  if (current == duration) {
    this.onMediaComplete_();
  }
};

MediaControls.prototype.onMediaError_ = function(e) {
// TODO: process the error
};

MediaControls.prototype.onMediaComplete_ = function(e) {
  this.onMediaPlay_(false);
};

/**
 * Create a customized slider control.
 *
 * @param {HTMLElement} container The containing div element.
 * @param {number} value Initial value [0..1].
 * @param {number} range Number of distinct slider positions to be supported.
 * @constructor
 */

MediaControls.Slider = function(container, value, range) {
  this.container_ =  container;
  var document = this.container_.ownerDocument;

  this.input_ = document.createElement('input');
  this.input_.type = 'range';
  this.input_.className = 'thumb';
  this.input_.min = 0;
  this.input_.max = range;
  this.input_.value = value * range;
  this.container_.appendChild(this.input_);

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
