// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Prototype for first-run tutorial steps.
 */

cr.define('cr.FirstRun', function() {
  var Step = cr.ui.define('div');

  Step.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Name of step.
    name_: null,

    // Button leading to next tutorial step.
    nextButton_: null,

    decorate: function() {
      this.name_ = this.getAttribute('id');
      this.nextButton_ = this.getElementsByClassName('next-button')[0];
      if (!this.nextButton_)
        throw Error('Next button not found.');
      this.nextButton_.addEventListener('click', (function(e) {
        chrome.send('nextButtonClicked', [this.getName()]);
        e.stopPropagation();
      }).bind(this));
    },

    getName: function() {
      return this.name_;
    }
  };

  return {Step: Step};
});
