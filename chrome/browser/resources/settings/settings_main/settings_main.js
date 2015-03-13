// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-main' displays the selected settings page.
 *
 * Example:
 *
 *     <cr-settings-main pages="{{pages}}" selectedPageId="{{selectedId}}">
 *     </cr-settings-main>
 *
 * See cr-settings-drawer for example of use in 'core-drawer-panel'.
 *
 * @group Chrome Settings Elements
 * @element cr-settings-main
 */
Polymer('cr-settings-main', {
  publish: {
    /**
     * Preferences state.
     *
     * @attribute prefs
     * @type CrSettingsPrefsElement
     * @default null
     */
    prefs: null,

    /**
     * Pages that may be shown.
     *
     * @attribute pages
     * @type Array<!Object>
     * @default null
     */
    pages: null,

    /**
     * ID of the currently selected page.
     *
     * @attribute selectedPageId
     * @type string
     * default ''
     */
    selectedPageId: '',
  },

  /** @override */
  created: function() {
    this.pages = [];
  },

  /** @override */
  ready: function() {
    var observer = new MutationObserver(this.pageContainerUpdated_.bind(this));
    observer.observe(this.$.pageContainer,
                     /** @type {MutationObserverInit} */ {
                       childList: true,
                     });
    this.pages = this.$.pageContainer.items;
    this.ensureSelection_();
  },

  /**
   * If no page is selected, selects the first page. This happens on load and
   * when a selected page is removed.
   *
   * @private
   */
  ensureSelection_: function() {
    if (!this.pages.length)
      return;
    if (this.selectedPageId == '')
      this.selectedPageId = this.pages[0].PAGE_ID;
  },

  /**
   * Updates the list of pages using the pages in core-animated-pages.
   *
   * @private
   */
  pageContainerUpdated_: function() {
    this.pages = this.$.pageContainer.items;
    this.ensureSelection_();
  },
});
