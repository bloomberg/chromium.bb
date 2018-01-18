// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @param {number} totalCount
   * @param {number} oppositeCount
   * @return {number}
   */
  function visibleLineCount(totalCount, oppositeCount) {
    // We limit the number of lines shown for DOM performance.
    const MAX_VISIBLE_LINES = 1000;
    const max =
        Math.max(MAX_VISIBLE_LINES / 2, MAX_VISIBLE_LINES - oppositeCount);
    return Math.min(max, totalCount);
  }

  const CodeSection = Polymer({
    is: 'extensions-code-section',

    properties: {
      /**
       * The code this object is displaying.
       * @type {?chrome.developerPrivate.RequestFileSourceResponse}
       */
      code: {
        type: Object,
        value: null,
      },

      /**
       * The text of the entire source file. This value does not update on
       * highlight changes; it only updates if the content of the source
       * changes.
       * @private
       */
      codeText_: String,

      /** @private */
      lineNumbers_: String,

      /** @private */
      truncatedBefore_: Number,

      /** @private */
      truncatedAfter_: Number,

      /**
       * The string to display if no |code| is set (e.g. because we couldn't
       * load the relevant source file).
       * @type {string}
       */
      couldNotDisplayCode: String,
    },

    observers: [
      'onCodeChanged_(code.*)',
    ],

    /**
     * @private
     */
    onCodeChanged_: function() {
      if (!this.code ||
          (!this.code.beforeHighlight && !this.code.highlight &&
           !this.code.afterHighlight)) {
        this.codeText_ = '';
        this.lineNumbers_ = '';
        return;
      }

      const before = this.code.beforeHighlight;
      const highlight = this.code.highlight;
      const after = this.code.afterHighlight;

      const linesBefore = before ? before.split('\n') : [];
      const linesAfter = after ? after.split('\n') : [];
      const visibleLineCountBefore =
          visibleLineCount(linesBefore.length, linesAfter.length);
      const visibleLineCountAfter =
          visibleLineCount(linesAfter.length, linesBefore.length);

      const visibleBefore =
          linesBefore.slice(linesBefore.length - visibleLineCountBefore)
              .join('\n');
      let visibleAfter = linesAfter.slice(0, visibleLineCountAfter).join('\n');
      // If the last character is a \n, force it to be rendered.
      if (visibleAfter.charAt(visibleAfter.length - 1) == '\n')
        visibleAfter += ' ';

      this.codeText_ = visibleBefore + highlight + visibleAfter;
      this.truncatedBefore_ = linesBefore.length - visibleLineCountBefore;
      this.truncatedAfter_ = linesAfter.length - visibleLineCountAfter;

      this.setLineNumbers_(
          this.truncatedBefore_ + 1,
          this.truncatedBefore_ + this.codeText_.split('\n').length);
      this.createHighlight_(
          visibleBefore.length, visibleBefore.length + highlight.length);
      this.scrollToHighlight_(visibleLineCountBefore);
    },

    /**
     * @param {number} lineCount
     * @param {string} stringSingular
     * @param {string} stringPluralTemplate
     * @return {string}
     * @private
     */
    getLinesNotShownLabel_(lineCount, stringSingular, stringPluralTemplate) {
      return lineCount == 1 ?
          stringSingular :
          loadTimeData.substituteString(stringPluralTemplate, lineCount);
    },

    /**
     * @param {number} start
     * @param {number} end
     * @private
     */
    setLineNumbers_: function(start, end) {
      let lineNumbers = '';
      for (let i = start; i <= end; ++i)
        lineNumbers += i + '\n';

      this.lineNumbers_ = lineNumbers;
    },

    /**
     * Uses the native text-selection API to highlight desired code.
     * @param {number} start
     * @param {number} end
     * @private
     */
    createHighlight_: function(start, end) {
      const range = document.createRange();
      const node = this.$.source.querySelector('span').firstChild;
      range.setStart(node, start);
      range.setEnd(node, end);

      const selection = window.getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
    },

    /**
     * @param {number} linesBeforeHighlight
     * @private
     */
    scrollToHighlight_: function(linesBeforeHighlight) {
      const CSS_LINE_HEIGHT = 20;

      // Count how many pixels is above the highlighted code.
      const highlightTop = linesBeforeHighlight * CSS_LINE_HEIGHT;

      // Find the position to show the highlight roughly in the middle.
      const targetTop = highlightTop - this.clientHeight * 0.5;

      this.$['scroll-container'].scrollTo({top: targetTop});
    },
  });

  return {CodeSection: CodeSection};
});
