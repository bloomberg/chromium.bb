// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Provides the UI to start and stop RTP recording, forwards the start/stop
 * commands to Chrome, and updates the UI based on dump updates.
 *
 * @param {Element} containerElement The parent element of the dump creation UI.
 */
var DumpCreator = (function() {
  /**
   * @param {Element} containerElement The parent element of the dump creation
   *     UI.
   * @constructor
   */
  function DumpCreator(containerElement) {
    /**
     * True if the RTP packets are being recorded.
     * @type {bool}
     * @private
     */
    this.recording_ = false;

    /**
     * @type {!Object.<string>}
     * @private
     * @const
     */
    this.StatusStrings_ = {
      NOT_STARTED: 'not started.',
      RECORDING: 'recording...',
    },

    /**
     * The status of dump creation.
     * @type {string}
     * @private
     */
    this.status_ = this.StatusStrings_.NOT_STARTED;

    /**
     * The root element of the dump creation UI.
     * @type {Element}
     * @private
     */
    this.root_ = document.createElement('details');

    this.root_.className = 'peer-connection-dump-root';
    containerElement.appendChild(this.root_);
    var summary = document.createElement('summary');
    this.root_.appendChild(summary);
    summary.textContent = 'Create Dump';
    var content = document.createElement('pre');
    this.root_.appendChild(content);
    content.innerHTML = '<button></button> Status: <span></span>';
    content.getElementsByTagName('button')[0].addEventListener(
        'click', this.onToggled_.bind(this));

    this.updateDisplay_();
  }

  DumpCreator.prototype = {
    /**
     * Handles the event of toggling the dump recording state.
     *
     * @private
     */
    onToggled_: function() {
      if (this.recording_) {
        this.recording_ = false;
        this.status_ = this.StatusStrings_.NOT_STARTED;
        chrome.send('stopRtpRecording');
      } else {
        this.recording_ = true;
        this.status_ = this.StatusStrings_.RECORDING;
        chrome.send('startRtpRecording');
      }
      this.updateDisplay_();
    },

    /**
     * Updates the UI based on the recording status.
     *
     * @private
     */
    updateDisplay_: function() {
      if (this.recording_) {
        this.root_.getElementsByTagName('button')[0].textContent =
            'Stop Recording RTP Packets';
      } else {
        this.root_.getElementsByTagName('button')[0].textContent =
            'Start Recording RTP Packets';
      }

      this.root_.getElementsByTagName('span')[0].textContent = this.status_;
    },

    /**
     * Set the status to the content of the update.
     * @param {!Object} update
     */
    onUpdate: function(update) {
      if (this.recording_) {
        this.status_ = JSON.stringify(update);
        this.updateDisplay_();
      }
    },
  };
  return DumpCreator;
})();
