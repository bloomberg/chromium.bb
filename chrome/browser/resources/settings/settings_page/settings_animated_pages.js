// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-animated-pages' is a container for a page and animated subpages.
 * It provides a set of common behaviors and animations. Notably, it provides
 * an 'inSubpage' property that indicates when a subpage is active.
 *
 * Example:
 *
 *    <cr-settings-animated-pages>
 *      <!-- Insert your section controls here -->
 *    </cr-settings-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-animated-pages
 */
Polymer({
  is: 'cr-settings-animated-pages',

  properties: {
    inSubpage: {
      type: Boolean,
      notify: true,
      observer: 'inSubpageChanged_',
    },
  },

  created: function() {
    this.history_ = ['main'];
  },

  /** @private */
  inSubpageChanged_: function() {
    this.classList.toggle('in-subpage', this.inSubpage);
  },

  navigateTo: function(page) {
    if (this.inSubpage) {
      this.$.animatedPages.exitAnimation = 'slide-left-animation';
      this.$.animatedPages.entryAnimation = 'slide-from-right-animation';
    } else {
      this.$.animatedPages.exitAnimation = 'settings-fade-out-animation';
      this.$.animatedPages.entryAnimation = 'settings-fade-in-animation';
    }

    this.history_.push(page);
    this.inSubpage = true;

    this.$.animatedPages.selected = page;
  },

  back: function() {
    this.history_.pop();
    this.inSubpage = this.history_.length > 1;

    if (this.inSubpage) {
      this.$.animatedPages.exitAnimation = 'slide-right-animation';
      this.$.animatedPages.entryAnimation = 'slide-from-left-animation';
    } else {
      this.$.animatedPages.exitAnimation = 'settings-fade-out-animation';
      this.$.animatedPages.entryAnimation = 'settings-fade-in-animation';
    }

    this.$.animatedPages.selected = this.history_.slice(-1)[0];
  },
});
