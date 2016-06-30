// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Introduces users to ChromeVox.
 */

goog.provide('Tutorial');

goog.require('Msgs');

/**
 * @constructor
 */
Tutorial = function() {
  /**
   * The 0-based index of the current page of the tutorial.
   * @type {number}
   * @private
   */
  this.page_ = 0;
};

/**
 * Data for the ChromeVox tutorial consisting of a list of pages,
 * each one of which contains a list of objects, where each object has
 * a message ID for some text, and optional attributes indicating if it's
 * a heading, link, text for only one platform, etc.
 *
 * @type Array<Object>
 */
Tutorial.PAGES = [
  [
    { msgid: 'tutorial_welcome_heading', heading: true },
    { msgid: 'tutorial_welcome_text' },
    { msgid: 'tutorial_enter_to_advance', repeat: true },
  ],
  [
    { msgid: 'tutorial_on_off_heading', heading: true },
    { msgid: 'tutorial_control' },
    { msgid: 'tutorial_on_off' },
    { msgid: 'tutorial_enter_to_advance', repeat: true },
  ],
  [
    { msgid: 'tutorial_modifier_heading', heading: true },
    { msgid: 'tutorial_modifier' },
    { msgid: 'tutorial_chromebook_search', chromebookOnly: true },
    { msgid: 'tutorial_any_key' },
    { msgid: 'tutorial_enter_to_advance', repeat: true },
  ],
  [
    { msgid: 'tutorial_basic_navigation_heading', heading: true },
    { msgid: 'tutorial_basic_navigation' },
    { msgid: 'tutorial_click_next' },
  ],
  [
    { msgid: 'tutorial_jump_heading', heading: true },
    { msgid: 'tutorial_jump' },
    { msgid: 'tutorial_click_next' },
  ],
  [
    { msgid: 'tutorial_menus_heading', heading: true },
    { msgid: 'tutorial_menus' },
    { msgid: 'tutorial_click_next' },
  ],
  [
    { msgid: 'tutorial_chrome_shortcuts_heading', heading: true },
    { msgid: 'tutorial_chrome_shortcuts' },
    { msgid: 'tutorial_chromebook_ctrl_forward', chromebookOnly: true },
    { msgid: 'tutorial_chromeos_ctrl_f2', chromebookOnly: false },
  ],
  [
    { msgid: 'tutorial_learn_more_heading', heading: true },
    { msgid: 'tutorial_learn_more' },
    { msgid: 'next_command_reference',
      link: 'http://www.chromevox.com/next_keyboard_shortcuts.html' },
    { msgid: 'chrome_keyboard_shortcuts',
      link: 'https://support.google.com/chromebook/answer/183101?hl=en' },
    { msgid: 'touchscreen_accessibility',
      link: 'https://support.google.com/chromebook/answer/6103702?hl=en' },
  ],
];

Tutorial.prototype = {
  /** Open the first page in the tutorial. */
  firstPage: function() {
    this.page_ = 0;
    this.showPage_();
  },

  /** Move to the next page in the tutorial. */
  nextPage: function() {
    if (this.page_ < Tutorial.PAGES.length - 1) {
      this.page_++;
      this.showPage_();
    }
  },

  /** Move to the previous page in the tutorial. */
  previousPage: function() {
    if (this.page_ > 0) {
      this.page_--;
      this.showPage_();
    }
  },

  /**
   * Recreate the tutorial DOM for page |this.page_|.
   * @private
   */
  showPage_: function() {
    var tutorialContainer = $('tutorial_main');
    tutorialContainer.innerHTML = '';

    var pageElements = Tutorial.PAGES[this.page_];
    for (var i = 0; i < pageElements.length; ++i) {
      var pageElement = pageElements[i];
      var msgid = pageElement.msgid;
      var text = Msgs.getMsg(msgid);
      var element;
      if (pageElement.heading) {
        element = document.createElement('h2');
      } else if (pageElement.link) {
        element = document.createElement('a');
        element.href = pageElement.link;
        element.target = '_blank';
      } else {
        element = document.createElement('p');
      }
      element.innerText = text;
      tutorialContainer.appendChild(element);
    }
  }
};
