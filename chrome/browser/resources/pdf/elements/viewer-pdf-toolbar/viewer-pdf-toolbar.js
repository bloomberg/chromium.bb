// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function() {
  /**
   * @type {Object}
   * core-meta object used to determine type of transition used.
   */
  var meta = document.createElement('core-meta');
  meta.type = 'transition';

  /**
   * @type {Object}
   * Sets up, starts and destroys the transition used.
   * This has no state and therefore is OK to make global.
   */
  var transition;

  Polymer('viewer-pdf-toolbar', {
    /**
     * @type {string}
     * The title of the PDF document.
     */
    docTitle: '',

    /**
     * @type {number}
     * The current index of the page being viewed (0-based).
     */
    pageIndex: 0,

    /**
     * @type {number}
     * The current loading progress of the PDF document (0 - 100).
     */
    loadProgress: 0,

    /**
     * @type {boolean}
     * Whether the document has bookmarks.
     */
    hasBookmarks: false,

    /**
     * @type {number}
     * The number of pages in the PDF document.
     */
    docLength: 1,

    ready: function() {
      // We cannot initialise transition before ensuring that core-meta has been
      // imported.
      if (transition === undefined)
        transition = meta.byId('core-transition-top');
      transition.setup(this.$.animatable);

      /**
       * @type {Object}
       * Used in core-transition to determine whether the animatable is open.
       */
      this.state_ = { opened: false };
      this.show();
    },

    loadProgressChanged: function() {
      if (this.loadProgress >= 100) {
        this.$.title.classList.toggle('invisible', false);
        this.$.pageselector.classList.toggle('invisible', false);
        this.$.buttons.classList.toggle('invisible', false);
      }
    },

    hide: function() {
      if (this.state_.opened)
        this.toggleVisibility();
    },

    show: function() {
      if (!this.state_.opened)
        this.toggleVisibility();
    },

    toggleVisibility: function() {
      this.state_.opened = !this.state_.opened;
      transition.go(this.$.animatable, this.state_);
    },

    selectPageNumber: function() {
      this.$.pageselector.select();
    },

    rotateRight: function() {
      this.fire('rotate-right');
    },

    toggleBookmarks: function() {
      this.fire('toggle-bookmarks');
    },

    save: function() {
      this.fire('save');
    },

    print: function() {
      this.fire('print');
    }
  });
})();
