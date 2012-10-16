// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(criterion) {
  return document.querySelector(criterion);
}

var cyclerUI = new (function () {

  /**
   * Enum for different UI states.
   * @enum {number}
   * @private
   */
  var EnableState_ = {capture: 0, playback: 1};

  this.cyclerData_ = new CyclerData();

  // Members for all UI elements subject to programmatic adjustment.
  this.captureTabLabel_ = $('#capture-tab-label');
  this.playbackTabLabel_ = $('#playback-tab-label');

  this.playbackTab_ = new PlaybackTab(this, this.cyclerData_);
  this.captureTab_ = new CaptureTab(this, this.cyclerData_, this.playbackTab_);

  this.popupDialog_ = $('#popup');
  this.popupContent_ = $('#popup-content');
  this.doPopupDismiss_ = $('#do-popup-dismiss');

  /**
   * Name of the most recent capture made, or the one most recently chosen
   * for playback.
   * @type {!string}
   */
  this.currentCaptureName = null;

  /**
   * One of the EnableState_ values, showing which tab is presently
   * enabled.
   * @type {number}
   */
  this.enableState = null;

  /*
   * Enable the capture tab, changing tab labels approproiately.
   * @private
   */
  this.enableCapture_ = function() {
    if (this.enableState != EnableState_.capture) {
      this.enableState = EnableState_.capture;

      this.captureTab_.enable();
      this.playbackTab_.disable();
    }
  };

  /*
   * Enable the playback tab, changing tab labels approproiately.
   * @private
   */
  this.enablePlayback_ = function() {
    if (this.enableState != EnableState_.playback) {
      this.enableState = EnableState_.playback;

      this.captureTab_.disable();
      this.playbackTab_.enable();
    }
  };

  /**
   * Show an overlay with a message, a dismiss button with configurable
   * label, and an action to call upon dismissal.
   * @param {!string} content The message to display.
   * @param {!string} dismissLabel The label on the dismiss button.
   * @param {function()} action Additional action to take, if any, upon
   *     dismissal.
   */
  this.showMessage = function(content, dismissLabel, action) {
    this.popupContent_.innerText = content;
    this.doPopupDismiss_.innerText = dismissLabel;
    this.popupDialog_.hidden = false;
    if (action != null)
      doPopupDismiss_.addEventListener('click', action);
  }

  /**
   * Default action for popup dismissal button, performed in addition to
   * any other actions that may be specified in showMessage_ call.
   * @private
   */
  this.clearMessage_ = function() {
    this.popupDialog_.hidden = true;
  }

  // Set up listeners on all buttons.
  this.doPopupDismiss_.addEventListener('click', this.clearMessage_.bind(this));

  // Set up listeners on tab labels.
  this.captureTabLabel_.addEventListener('click',
      this.enableCapture_.bind(this));
  this.playbackTabLabel_.addEventListener('click',
      this.enablePlayback_.bind(this));

  // Start with capture tab displayed.
  this.enableCapture_();
})();
