// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-list-item',

  properties: {
    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @private {string} */
    searchHint_: {
      type: String,
      notify: true,
    },

    /** @type {boolean} */
    stale: {
      type: Boolean,
      notify: true,
      reflectToAttribute: true,
    },
  },

  observers: [
    'onDestinationPropertiesChange_(' +
        'destination.displayName, destination.isOfflineOrInvalid)',
  ],

  /** @private {boolean} */
  highlighted_: false,

  /** @private */
  onDestinationPropertiesChange_: function() {
    this.title = this.destination.displayName;
    this.stale = this.destination.isOfflineOrInvalid;
  },

  /** @param {?RegExp} searchQuery The search query to update for. */
  update: function(searchQuery) {
    this.updateSearchHint_(searchQuery);
    this.updateHighlighting_(searchQuery);
  },

  /**
   * @param {?RegExp} searchQuery The search query to update the hint for.
   * @private
   */
  updateSearchHint_: function(searchQuery) {
    if (!searchQuery) {
      this.searchHint_ = '';
      return;
    }
    this.searchHint_ = this.destination.extraPropertiesToMatch
                           .filter(p => p.match(searchQuery))
                           .join(' ');
  },

  /**
   * @param {?RegExp} searchQuery The search query to update the hint for.
   * @private
   */
  updateHighlighting_: function(searchQuery) {
    if (this.highlighted_) {
      cr.search_highlight_utils.findAndRemoveHighlights(this);
      this.highlighted_ = false;
    }

    if (!searchQuery)
      return;

    this.shadowRoot.querySelectorAll('.searchable').forEach(element => {
      element.childNodes.forEach(node => {
        if (node.nodeType != Node.TEXT_NODE)
          return;

        const textContent = node.nodeValue.trim();
        if (textContent.length == 0)
          return;

        if (searchQuery.test(textContent)) {
          // Don't highlight <select> nodes, yellow rectangles can't be
          // displayed within an <option>.
          // TODO(rbpotter): solve issue below before adding advanced
          // settings so that this function can be re-used.
          // TODO(dpapad): highlight <select> controls with a search bubble
          // instead.
          if (node.parentNode.nodeName != 'OPTION') {
            cr.search_highlight_utils.highlight(
                node, textContent.split(searchQuery));
            this.highlighted_ = true;
          }
        }
      });
    });
  },
});
