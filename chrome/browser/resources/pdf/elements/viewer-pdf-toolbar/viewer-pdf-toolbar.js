// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function() {
  Polymer({
    is: 'viewer-pdf-toolbar',

    behaviors: [
      Polymer.NeonAnimationRunnerBehavior
    ],

    properties: {
      /**
       * The current loading progress of the PDF document (0 - 100).
       */
      loadProgress: {
        type: Number,
        observer: 'loadProgressChanged'
      },

      /**
       * The title of the PDF document.
       */
      docTitle: String,

      /**
       * The number of the page being viewed (1-based).
       */
      pageNo: Number,

      /**
       * Whether the document has bookmarks.
       */
      hasBookmarks: Boolean,

      /**
       * The number of pages in the PDF document.
       */
      docLength: Number,

      /**
       * Whether the toolbar is opened and visible.
       */
      opened: {
        type: Boolean,
        value: true
      },

      animationConfig: {
        value: function() {
          return {
            'entry': {
              name: 'slide-down-animation',
              node: this,
              timing: {
                easing: 'cubic-bezier(0, 0, 0.2, 1)',
                duration: 250
              }
            },
            'exit': {
              name: 'slide-up-animation',
              node: this,
              timing: {
                easing: 'cubic-bezier(0.4, 0, 1, 1)',
                duration: 250
              }
            }
          };
        }
      }
    },

    listeners: {
      'neon-animation-finish': '_onAnimationFinished'
    },

    _onAnimationFinished: function() {
      if (!this.opened)
        this.style.visibility = 'hidden';
    },

    loadProgressChanged: function() {
      if (this.loadProgress >= 100) {
        this.$.title.classList.toggle('invisible', false);
        this.$.pageselector.classList.toggle('invisible', false);
        this.$.buttons.classList.toggle('invisible', false);
      }
    },

    hide: function() {
      if (this.opened)
        this.toggleVisibility();
    },

    show: function() {
      if (!this.opened) {
        this.toggleVisibility();
        this.style.visibility = 'initial';
      }
    },

    toggleVisibility: function() {
      this.opened = !this.opened;
      this.cancelAnimation();
      this.playAnimation(this.opened ? 'entry' : 'exit');
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
