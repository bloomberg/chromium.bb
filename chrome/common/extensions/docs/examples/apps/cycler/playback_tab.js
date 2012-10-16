// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Constructor for the tab UI governing playback selection and running.
 * All HTML controls under tag #plaback-tab, plus the tab label
 * #playback-tab-label are controlled by this class.
 * @param {!Object} cyclerUI  The master UI class, needed for global state
 *     such as current capture name.
 * @param {!Object} cyclerData  The local FileSystem-based database of
 *     available captures to play back.
 */
var PlaybackTab = function (cyclerUI, cyclerData) {
  /**
   * Enum for different playback tab states.
   * @enum {number}
   * @private
   */
  var EnableState_ = {
    choosePlayback: 1,  // Choose a playback, if none already chosen.
    showPlayback: 2,  // Show currently chosen playback, and offer options.
    showNoCaptures: 3  // Show message indicating no captures are available.
  };

  this.cyclerUI_ = cyclerUI;
  this.cyclerData_ = cyclerData;

  /**
   * Create members for all UI elements with which we'll interact.
   */
  this.tabLabel_ = $('#playback-tab-label');
  this.playbackTab_ = $('#playback-tab');
  this.noCaptures_ = $('#no-captures');
  this.yesCaptures_ = $('#have-captures');

  this.playbackName_ = $('#playback-name');
  this.playbackDetails_ = $('#playback-details');
  this.playbackURLs_ = $('#playback-urls');
  this.playbackRepeats_ = $('#playback-repeats');
  this.playbackExtension_ = $('#playback-extension');
  this.doPlaybackButton_ = $('#do-playback');
  this.doDeleteButton_ = $('#do-delete');

  /*
   * Enable the playback tab, showing no current playback choice, by
   * hiding the playbackDetails_ div.
   * @private
   */
  this.enableChoosePlayback_ = function() {
    if (this.enableState != EnableState_.choosePlayback) {
      this.enableState = EnableState_.choosePlayback;
      this.yesCaptures_.hidden = false;
      this.noCaptures_.hidden = true;
      this.playbackDetails_.hidden = true;
    }
  };

  /*
   * Enable the playback tab, showing a current playback choice by showing
   * the playbackDetails_ div.
   * @private
   */
  this.enableShowPlayback_ = function() {
    if (this.enableState != EnableState_.showPlayback) {
      this.enableState = EnableState_.showPlayback;
      this.yesCaptures_.hidden = false;
      this.noCaptures_.hidden = true;
      this.playbackDetails_.hidden = false;
    }
  };

  /*
   * Enable the playback tab and adjust tab labels appropriately. Show
   * no available captures by hiding the yesCaptures_ div and showing the
   * noCaptures_ div instead.
   * @private
   */
  this.enableShowNoCaptures_ = function() {
    if (this.enableState != EnableState_.showNoCaptures) {
      this.enableState = EnableState_.showNoCaptures;
      this.noCaptures_.hidden = false;
      this.yesCaptures_.hidden = true;
    }
  };

  /**
   * Enable the playback tab, showing either its "no captures", "choose
   * a capture" or "display chosen capture" form, depending on the state
   * of existing captures and |currentCaptureName_|.
   */
  this.enable = function() {
    this.tabLabel_.classList.add('selected');
    this.updatePlaybackChoices_();

    this.playbackTab_.hidden = false;
    if (this.cyclerData_.getCaptures().length == 0) {
      this.enableShowNoCaptures_();
    } else if (this.cyclerUI_.currentCaptureName == null) {
      this.enableChoosePlayback_();
    } else {
      this.enableShowPlayback_();
    }
  }

  /**
   * Disable the playback tab altogether, presumably in favor of some
   * other tab.
   */
  this.disable = function() {
    this.tabLabel_.classList.remove('selected');
    this.playbackTab_.hidden = true;
  }

  /**
   * Utility function to refresh the selection list of captures that may
   * be chosen for playback. Show all current captures, and also a
   * "Choose a capture" default text if no capture is currently selected.
   * @private
   */
  this.updatePlaybackChoices_ = function() {
    var captureNames = this.cyclerData_.getCaptures();
    var options = this.playbackName_.options;
    var nextIndex = 0;
    var chooseOption;

    options.length = 0;

    if (this.cyclerUI_.currentCaptureName == null) {
      chooseOption = new Option('Choose a capture', null);
      chooseOption.disabled = true;
      options.add(chooseOption);
      options.selectedIndex = nextIndex++;
    }
    for (var i = 0; i < captureNames.length; i++) {
      options.add(new Option(captureNames[i], captureNames[i]));
      if (captureNames[i] == this.cyclerUI_.currentCaptureName) {
        options.selectedIndex = nextIndex;
      }
      nextIndex++;
    }
  }

  /**
   * Event callback for selection of a capture to play back. Save the
   * choice in |currentCaptureName_|. Update the selection list because the
   * default reminder message is no longer needed when a capture is chosen.
   * Change playback tab to show-chosen-capture mode.
   */
  this.selectPlaybackName = function() {
    this.cyclerUI_.currentCaptureName = this.playbackName_.value;
    this.updatePlaybackChoices_();
    this.enableShowPlayback_();
  }

  /**
   * Event callback for pressing the playback button, which button is only
   * enabled if a capture has been chosen for playback. Check for errors
   * on playback page, and either display a message with such errors, or
   * call the extenion api to initiate a playback.
   * @private
   */
  this.doPlayback_ = function() {
    var extensionPath = this.playbackExtension_.value;
    var repeatCount = parseInt(this.playbackRepeats_.value);
    var errors = [];

    // Check local errors
    if (isNaN(repeatCount)) {
      errors.push('Enter a number for repeat count');
    } else if (repeatCount < 1 || repeatCount > 100) {
      errors.push('Repeat count must be between 1 and 100');
    }

    if (errors.length > 0) {
      this.cyclerUI_.showMessage(errors.join('\n'), 'Ok');
    } else {
      this.doPlaybackButton_.disabled = true;
      chrome.experimental.record.replayURLs(
          this.cyclerUI_.currentCaptureName,
          repeatCount,
          {'extensionPath': extensionPath},
          this.onPlaybackDone.bind(this));
    }
  }

  /**
   * Extension API calls this back when a playback is done.
   * @param {!{
   *   runTime: number,
   *   stats: string,
   *   errors: !Array.<string>
   * }} results  The results of the playback, including running time in ms,
   *     a string of statistics information, and a string array of errors.
   */
  this.onPlaybackDone = function(results) {
    this.doPlaybackButton_.disabled = false;

    if (results.errors.length > 0) {
      this.cyclerUI_.showMessage(results.errors.join('\n'), 'Ok');
    } else {
      this.cyclerUI_.showMessage('Test took ' + results.runTime + 'mS :\n' +
          results.stats, 'Ok');
    }
  }

  /**
   * Delete the capture with name |currentCaptureName_|. For this method
   * to be callable, there must be a selected currentCaptureName_. Change
   * the displayed HTML to show no captures if none exist now, or to show
   * none selected (since we just deleted the selected one).
   * @private
   */
  this.doDelete_ = function() {
    this.cyclerData_.deleteCapture(this.cyclerUI_.currentCaptureName);
    this.cyclerUI_.currentCaptureName = null;
    this.updatePlaybackChoices_();
    if (this.cyclerData_.getCaptures().length == 0) {
      this.enableShowNoCaptures_();
    } else {
      this.enableChoosePlayback_();
    }
  }

  // Set up listeners on buttons.
  this.doPlaybackButton_.addEventListener('click', this.doPlayback_.bind(this));
  this.doDeleteButton_.addEventListener('click', this.doDelete_.bind(this));

  // Set up initial selection list for existing captures, and selection
  // event listener.
  this.updatePlaybackChoices_();
  this.playbackName_.addEventListener('change',
      this.selectPlaybackName.bind(this));
};
