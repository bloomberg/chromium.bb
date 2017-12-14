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
       * The text of the entire source file. This value does not update on
       * highlight changes; it only updates if the content of the source
       * changes.
       * @private
       */
      codeText_: {
        type: String,
        computed: 'computeCodeText_(code.*)',
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
     * @return {string}
     * @private
     */
    computeCodeText_: function() {
      if (!this.code)
        return '';

      return this.code.beforeHighlight + this.code.highlight +
          this.code.afterHighlight;
    },

    /**
     * Computes the content of the line numbers span, which basically just
     * contains 1\n2\n3\n... for the number of lines.
     * @return {string}
     * @private
     */
    computeLineNumbersContent_: function() {
      if (!this.codeText_)
        return '';

      const lines = this.codeText_.match(/\n/g);
      const lineCount = lines ? lines.length : 0;
      let textContent = '';
      for (let i = 1; i <= lineCount + 1; ++i)
        textContent += i + '\n';
      return textContent;
    },

    /**
     * Uses the native text-selection API to highlight desired code.
     * @private
     */
    createHighlight_: function() {
      const range = document.createRange();
      const node = this.$.source.querySelector('span').firstChild;
      range.setStart(node, this.code.beforeHighlight.length);
      range.setEnd(
          node, this.code.beforeHighlight.length + this.code.highlight.length);

      const selection = window.getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
    },

    /**
     * Scroll the highlight code to roughly the middle. It will do smooth
     * scrolling if the target scroll position is close-by, or jump to it if
     * it's far away.
     * @private
     */
    scrollToHighlight_: function() {
      const CSS_LINE_HEIGHT = 20;
      const SCROLL_LOC_THRESHOLD = 100;

      const linesBeforeHighlight = this.code.beforeHighlight.match(/\n/g);

      // Count how many pixels is above the highlighted code.
      const highlightTop = linesBeforeHighlight ?
          linesBeforeHighlight.length * CSS_LINE_HEIGHT :
          0;

      // Find the position to show the highlight roughly in the middle.
      const targetTop = highlightTop - this.clientHeight * 0.5;

      // Smooth scrolling if moving within ~100 LOC, otherwise just jump to it.
      const behavior =
          Math.abs(this.$['scroll-container'].scrollTop - targetTop) <
              (CSS_LINE_HEIGHT * SCROLL_LOC_THRESHOLD) ?
          'smooth' :
          'auto';
      this.$['scroll-container'].scrollTo({
        top: targetTop,
        behavior: behavior,
      });
    },

    /** @private */
    onHighlightChanged_: function() {
      if (!this.codeText_)
        return;

      this.createHighlight_();
      this.scrollToHighlight_();
    },
  });

  return {CodeSection: CodeSection};
});
