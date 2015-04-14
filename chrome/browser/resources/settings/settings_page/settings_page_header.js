// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-page-header' shows a basic heading with a 'core-icon'.
 *
 * Example:
 *
 *    <cr-settings-page-header page="{{page}}">
 *    </cr-settings-page-header>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-page-header
 */
Polymer('cr-settings-page-header', {
  publish: {
    /**
     * Page to show a header for.
     *
     * @attribute page
     * @type Object
     * @default null
     */
    page: null,

    /**
     * The current stack of pages being viewed. I.e., may contain subpages.
     *
     * @attribute pageStack
     * @type Array
     * @default null
     */
    pageStack: null,

    /**
     * The selector determining which page is currently selected.
     *
     * @attribute selector
     * @type HTMLElement
     * @default null
     */
    selector: null,
  },

  ready: function() {
    this.pageStack = [];
  },

  selectorChanged: function() {
    this.selector.addEventListener('core-select', function() {
      // core-select gets fired once for deselection and once for selection;
      // ignore the deselection case.
      if (!this.selector.selectedItem)
        return;

      if (this.selector.selectedItem.subpage) {
        this.pageStack.push(this.selector.selectedItem);
      } else {
        this.pageStack = [this.selector.selectedItem];
      }
    }.bind(this));
  }
});
