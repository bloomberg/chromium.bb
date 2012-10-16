// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Constructor for the tab UI governing setup and initial running of capture
 * baselines. All HTML controls under tag #capture-tab, plus the tab label
 * #capture-tab-label are controlled by this class.
 * @param {!Object} cyclerUI  The master UI class, needed for global state
 *     such as current capture name.
 * @param {!Object} cyclerData  The local FileSystem-based database of
 *     available captures to play back.
 * @param {!Object} playbackTab  The class governing playback selections.
 *     We need this in order to update its choices when we get asynchronous
 *     callbacks for successfully completed capture baselines.
 */
var CaptureTab = function (cyclerUI, cyclerData, playbackTab) {
  // Members for all UI elements subject to programmatic adjustment.
  this.tabLabel_ = $('#capture-tab-label');
  this.captureTab_ = $('#capture-tab');
  this.captureName_ = $('#capture-name');
  this.captureURLs_ = $('#capture-urls');
  this.doCaptureButton_ = $('#do-capture');

  // References to other major components of the extension.
  this.cyclerUI_ = cyclerUI;
  this.cyclerData_ = cyclerData;
  this.playbackTab_ = playbackTab;

  /*
   * Enable the capture tab and its label.
   */
  this.enable = function() {
    this.captureTab_.hidden = false;
    this.tabLabel_.classList.add('selected');
  };

  /*
   * Disable the capture tab and its label.
   */
  this.disable = function() {
    this.captureTab_.hidden = true;
    this.tabLabel_.classList.remove('selected');
  };

  /**
   * Do a capture using current data from the capture tab. Post an error
   * dialog if said data is incorrect or incomplete. Otherwise pass
   * control to the browser side.
   * @private
   */
  this.doCapture_ = function() {
    var errors = [];
    var captureName = this.captureName_.value.trim();
    var urlList;

    urlList = this.captureURLs_.value.split('\n');
    if (captureName.length == 0)
      errors.push('Must give a capture name');
    if (urlList.length == 0)
      errors.push('Must give at least one URL');

    if (errors.length > 0) {
      this.cyclerUI_.showMessage(errors.join('\n'), 'Ok');
    } else {
      this.doCaptureButton_.disabled = true;
      chrome.experimental.record.captureURLs(captureName, urlList,
          this.onCaptureDone.bind(this));
    }
  }

  /**
   * Callback for completed (or possibly failed) capture. Post a message
   * box, either with errors or "Success!" message.
   * @param {!Array.<string>} errors List of errors that occured
   *     during capture, if any.
   */
  this.onCaptureDone = function(errors) {
    this.doCaptureButton_.disabled = false;

    if (errors.length > 0) {
      this.cyclerUI_.showMessage(errors.join('\n'), 'Ok');
    } else {
      this.cyclerUI_.showMessage('Success!', 'Ok');
      this.cyclerUI_.currentCaptureName = this.captureName_.value.trim();
      this.cyclerData_.saveCapture(this.cyclerUI_.currentCaptureName);
    }
  }

  // Set up listener for capture button.
  this.doCaptureButton_.addEventListener('click', this.doCapture_.bind(this));
};
