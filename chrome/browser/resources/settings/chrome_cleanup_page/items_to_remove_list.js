// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * The default number of items to show for files and registry keys on the
 * detailed view when user-initiated cleanups are enabled.
 */
settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW = 4;

/**
 * @fileoverview
 * 'items-to-remove-list' represents a list of items to
 * be removed or changed to be shown on the Chrome Cleanup page.
 * TODO(crbug.com/776538): Update the strings to say that some items are only
 *                         changed and not removed.
 *
 * Examples:
 *
 *    <!-- Items list initially expanded. -->
 *    <items-to-remove-list
 *        title="Files and programs:"
 *        initially-expanded="true"
 *        items-to-show="[[filesToShow]]">
 *    </items-to-remove-list>
 *
 *    <!-- Items list initially shows |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW|
 *         items. If there are more than |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW|
 *         items on the list, then a "show more" link is shown; tapping on it
 *         expands the list. -->
 *    <items-to-remove-list
 *        title="Files and programs:"
 *        items-to-show="[[filesToShow]]">
 *    </items-to-remove-list>
 */
Polymer({
  is: 'items-to-remove-list',

  properties: {
    titleVisible: {
      type: Boolean,
      value: true,
    },

    title: {
      type: String,
      value: '',
    },

    /** @type {!Array<string>} */
    itemsToShow: Array,

    /**
     * If true, all items from |itemsToShow| will be presented on the card
     * by default, and the "show more" link will be omitted.
     */
    initiallyExpanded: {
      type: Boolean,
      value: false,
    },

    /**
     * If true, all items from |itemsToShow| will be presented on the card,
     * and the "show more" link will be omitted.
     */
    expanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * The list of items to actually present on the card. If |expanded_|, then
     * it's the same as |itemsToShow|.
     * @private {?Array<string>}
     */
    visibleItems_: Array,

    /**
     * The text for the "show more" link available if not all files are visible
     * in the card.
     * @private
     */
    moreItemsLinkText_: {
      type: String,
      value: '',
    },
  },

  observers: ['updateVisibleState_(itemsToShow, initiallyExpanded)'],

  /** @private */
  expandList_: function() {
    this.expanded_ = true;
    this.visibleItems_ = this.itemsToShow;
    this.moreItemsLinkText_ = '';
  },

  /**
   * Decides which elements will be visible in the card and if the "show more"
   * link will be rendered.
   *
   * Cases handled:
   *  1. If |initiallyExpanded|, then all items will be visible.
   *  2. Otherwise:
   *     (A) If size(itemsToShow) < CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW, then
   *         all items will be visible.
   *     (B) Otherwise, exactly |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1| will
   *         be visible and the "show more" link will be rendered. The list
   *         presented to the user will contain exactly
   *         |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW| elements, and the last one
   *         will be the "show more" link.
   *
   * @param {!Array<string>} itemsToShow
   * @param {boolean} initiallyExpanded
   */
  updateVisibleState_: function(itemsToShow, initiallyExpanded) {
    // Start expanded if there are less than
    // |settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW| items to show.
    this.expanded_ = this.initiallyExpanded ||
        this.itemsToShow.length <=
            settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW;

    if (this.expanded_) {
      this.visibleItems_ = this.itemsToShow;
      this.moreItemsLinkText_ = '';
      return;
    }

    this.visibleItems_ = this.itemsToShow.slice(
        0, settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1);

    const browserProxy = settings.ChromeCleanupProxyImpl.getInstance();
    browserProxy
        .getMoreItemsPluralString(
            this.itemsToShow.length - this.visibleItems_.length)
        .then(linkText => {
          this.moreItemsLinkText_ = linkText;
        });
  },
});
