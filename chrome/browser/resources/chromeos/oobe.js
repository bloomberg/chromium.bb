// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Out of the box experience flow (OOBE).
 * This is the main code for the OOBE WebUI implementation.
 */

const steps = ['connect', 'eula', 'update'];

cr.define('cr.ui', function() {
  /**
  * Constructs an Out of box controller. It manages initialization of screens,
  * transitions, error messages display.
  *
  * @constructor
  */
  function Oobe() {
  }

  cr.addSingletonGetter(Oobe);

  Oobe.localStrings_ = new LocalStrings();

  Oobe.prototype = {
    /**
     * Current OOBE step, index in the steps array.
     * @type {number}
     */
    currentStep_: 0,

    /**
     * Switches to the next OOBE step.
     * @param {number} nextStepIndex Index of the next step.
     */
    toggleStep_: function(nextStepIndex) {
      var currentStepId = steps[this.currentStep_];
      var nextStepId = steps[nextStepIndex];
      var oldStep = $(currentStepId);
      var oldHeader = $('header-' + currentStepId);
      var newStep = $(nextStepId);
      var newHeader = $('header-' + nextStepId);

      newStep.classList.remove('hidden');

      if (nextStepIndex > this.currentStep_) {
        oldHeader.classList.add('left');
        oldStep.classList.add('left');
        newHeader.classList.remove('right');
        newStep.classList.remove('right');
      } else if (nextStepIndex < this.currentStep_) {
        oldHeader.classList.add('right');
        oldStep.classList.add('right');
        newHeader.classList.remove('left');
        newStep.classList.remove('left');
      }

      // Adjust inner container height based on new step's height.
      $('inner-container').style.height = newStep.offsetHeight;

      oldStep.addEventListener('webkitTransitionEnd', function f(e) {
        oldStep.removeEventListener('webkitTransitionEnd', f);
        oldStep.classList.add('hidden');
      });
      this.currentStep_ = nextStepIndex;
      $('oobe').className = nextStepId;
    },
  };

  /**
   * Initializes the OOBE flow.  This will cause all C++ handlers to
   * be invoked to do final setup.
   */
  Oobe.initialize = function() {
    // Adjust inner container height based on first step's height
    $('inner-container').style.height = $(steps[0]).offsetHeight;

    $('continue-button').addEventListener('click', function(event) {
      // TODO(nkostylev): Callback screen handler.
      Oobe.toggleStep(1);
    });
    $('back-button').addEventListener('click', function(event) {
      // TODO(nkostylev): Callback screen handler.
      Oobe.toggleStep(0);
    });
    $('accept-button').addEventListener('click', function(event) {
      // TODO(nkostylev): Callback screen handler.
      Oobe.toggleStep(2);
    });

    chrome.send('screenStateInitialize');
  };

  /**
   * Switches to the next OOBE step.
   * @param {number} nextStepIndex Index of the next step.
   */
  Oobe.toggleStep = function(nextStepIndex) {
    Oobe.getInstance().toggleStep_(nextStepIndex);
  };

  /**
   * Sets update's progress bar value.
   * @param {number} percentage of the progress.
   */
  Oobe.setUpdateProgress = function(progress) {
    $('update-progress-bar').value = progress;
  }

  /**
   * Sets update message, which is shown above the progress bar.
   * @param {text} message to be shown by the label.
   */
  Oobe.setUpdateMessage = function(message) {
    $('update-upper-label').innerText = message;
  }

  /**
   * Shows or hides update curtain.
   * @param {bool} whether curtain should be shown.
   */
  Oobe.showUpdateCurtain = function(enable) {
    $('update-screen-curtain').hidden = !enable;
    $('update-screen-main').hidden = enable;
  }

  // Export
  return {
    Oobe: Oobe
  };
});

var Oobe = cr.ui.Oobe;

document.addEventListener('DOMContentLoaded', cr.ui.Oobe.initialize);
