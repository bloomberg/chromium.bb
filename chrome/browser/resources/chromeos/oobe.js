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
      chrome.send('networkOnExit', []);
    });
    $('back-button').addEventListener('click', function(event) {
      chrome.send('eulaOnExit', [false, $('usage-stats').checked]);
    });
    $('accept-button').addEventListener('click', function(event) {
      chrome.send('eulaOnExit', [true, $('usage-stats').checked]);
    });
    $('security-link').addEventListener('click', function(event) {
      chrome.send('eulaOnTpmPopupOpened', []);
      $('popup-overlay').hidden = false;
    });
    $('security-ok-button').addEventListener('click', function(event) {
      $('popup-overlay').hidden = true;
    });

    chrome.send('screenStateInitialize', []);
  };

  /**
   * Switches to the next OOBE step.
   * @param {number} nextStepIndex Index of the next step.
   */
  Oobe.toggleStep = function(nextStepIndex) {
    Oobe.getInstance().toggleStep_(nextStepIndex);
  };

  /**
   * Enables/disables continue button.
   * @param {bool} whether button should be enabled.
   */
  Oobe.enableContinueButton = function(enable) {
    $('continue-button').disabled = !enable;
  };

  /**
   * Sets usage statistics checkbox.
   * @param {bool} whether the checkbox is checked.
   */
  Oobe.setUsageStats = function(checked) {
    $('usage-stats').checked = checked;
  };

  /**
   * Set OEM EULA URL.
   * @param {text} OEM EULA URL.
   */
  Oobe.setOemEulaUrl = function(oem_eula_url) {
    if (oem_eula_url) {
      $('oem-eula-frame').src = oem_eula_url;
      $('eulas').classList.remove('one-column');
    } else {
      $('eulas').classList.add('one-column');
    }
  };

  /**
   * Sets update's progress bar value.
   * @param {number} percentage of the progress.
   */
  Oobe.setUpdateProgress = function(progress) {
    $('update-progress-bar').value = progress;
  };

  /**
   * Sets update message, which is shown above the progress bar.
   * @param {text} message to be shown by the label.
   */
  Oobe.setUpdateMessage = function(message) {
    $('update-upper-label').innerText = message;
  };

  /**
   * Shows or hides update curtain.
   * @param {bool} whether curtain should be shown.
   */
  Oobe.showUpdateCurtain = function(enable) {
    $('update-screen-curtain').hidden = !enable;
    $('update-screen-main').hidden = enable;
  };

  /**
   * Sets TPM password.
   * @param {text} TPM password to be shown.
   */
  Oobe.setTpmPassword = function(password) {
    $('tpm-busy').hidden = true;
    $('tpm-password').innerText = password;
    $('tpm-password').hidden = false;
  }

  // Export
  return {
    Oobe: Oobe
  };
});

var Oobe = cr.ui.Oobe;

document.addEventListener('DOMContentLoaded', cr.ui.Oobe.initialize);
