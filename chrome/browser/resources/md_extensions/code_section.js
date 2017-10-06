// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const CodeSection = Polymer({
    is: 'extensions-code-section',

    properties: {
      /**
       * The code this object is displaying.
       * @type {?chrome.developerPrivate.RequestFileSourceResponse}
       */
      code: {
        type: Object,
        // We initialize to null so that Polymer sees it as defined and calls
        // isMainHidden_().
        value: null,
      },

      /**
       * The string to display if no |code| is set (e.g. because we couldn't
       * load the relevant source file).
       * @type {string}
       */
      couldNotDisplayCode: String,
    },

    observers: [
      'onHighlightChanged_(code.highlight)',
    ],

    /**
     * Returns true if no code could be displayed (e.g. because the file could
     * not be loaded).
     * @return {boolean}
     */
    isEmpty: function() {
      return !this.code ||
          (!this.code.beforeHighlight && !this.code.highlight &&
           !this.code.afterHighlight);
    },

    /**
     * Computes the content of the line numbers span, which basically just
     * contains 1\n2\n3\n... for the number of lines.
     * @return {string}
     * @private
     */
    computeLineNumbersContent_: function() {
      if (!this.code)
        return '';

      const lines = [
        this.code.beforeHighlight, this.code.highlight, this.code.afterHighlight
      ].join('').match(/\n/g);
      const lineCount = lines ? lines.length : 0;
      let textContent = '';
      for (let i = 1; i <= lineCount; ++i)
        textContent += i + '\n';
      return textContent;
    },

    /** @private */
    onHighlightChanged_: function() {
      // Smooth scroll the highlight to roughly the middle.
      this.$['scroll-container'].scrollTo({
        top: this.$.highlight.offsetTop - this.clientHeight * 0.5,
        behavior: 'smooth',
      });
    },
  });

  return {CodeSection: CodeSection};
});
