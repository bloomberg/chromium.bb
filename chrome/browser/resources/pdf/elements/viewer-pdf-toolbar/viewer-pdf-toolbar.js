// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function() {
Polymer({
  is: 'viewer-pdf-toolbar',

  properties: {
    /**
     * The current loading progress of the PDF document (0 - 100).
     */
    loadProgress: {type: Number, observer: 'loadProgressChanged_'},

    /**
     * The title of the PDF document.
     */
    docTitle: String,

    /**
     * The number of the page being viewed (1-based).
     */
    pageNo: Number,

    /**
     * Tree of PDF bookmarks (or null if the document has no bookmarks).
     */
    bookmarks: {type: Object, value: null},

    /**
     * The number of pages in the PDF document.
     */
    docLength: Number,

    /**
     * Whether the toolbar is opened and visible.
     */
    opened: {type: Boolean, value: true},

    /**
     * Whether the viewer is currently in annotation mode.
     */
    annotationMode: {
      type: Boolean,
      notify: true,
    },

    /**
     * Whether the PDF Annotations feature is enabled.
     */
    pdfAnnotationsEnabled: Boolean,

    strings: Object,
  },

  /**
   * @param {number} newProgress
   * @param {number} oldProgress
   * @private
   */
  loadProgressChanged_: function(newProgress, oldProgress) {
    const loaded = newProgress >= 100;
    const progressReset = newProgress < oldProgress;
    if (progressReset || loaded) {
      this.$.pageselector.classList.toggle('invisible', !loaded);
      this.$.buttons.classList.toggle('invisible', !loaded);
      this.$.progress.style.opacity = loaded ? 0 : 1;
    }
  },

  hide: function() {
    if (this.opened)
      this.toggleVisibility();
  },

  show: function() {
    if (!this.opened) {
      this.toggleVisibility();
    }
  },

  toggleVisibility: function() {
    this.opened = !this.opened;

    // We keep a handle on the animation in order to cancel the filling
    // behavior of previous animations.
    if (this.animation_) {
      this.animation_.cancel();
    }

    if (this.opened) {
      this.animation_ = this.animate(
          {
            transform: ['translateY(-100%)', 'translateY(0%)'],
          },
          {
            easing: 'cubic-bezier(0, 0, 0.2, 1)',
            duration: 250,
            fill: 'forwards',
          });
    } else {
      this.animation_ = this.animate(
          {
            transform: ['translateY(0%)', 'translateY(-100%)'],
          },
          {
            easing: 'cubic-bezier(0.4, 0, 1, 1)',
            duration: 250,
            fill: 'forwards',
          });
    }
  },

  selectPageNumber: function() {
    this.$.pageselector.select();
  },

  shouldKeepOpen: function() {
    return this.$.bookmarks.dropdownOpen || this.loadProgress < 100 ||
        this.$.pageselector.isActive();
  },

  hideDropdowns: function() {
    if (this.$.bookmarks.dropdownOpen) {
      this.$.bookmarks.toggleDropdown();
      return true;
    }
    return false;
  },

  setDropdownLowerBound: function(lowerBound) {
    this.$.bookmarks.lowerBound = lowerBound;
  },

  rotateRight: function() {
    this.fire('rotate-right');
  },

  download: function() {
    this.fire('save');
  },

  print: function() {
    this.fire('print');
  },

  toggleAnnotation: function() {
    this.annotationMode = !this.annotationMode;
  }
});
})();
