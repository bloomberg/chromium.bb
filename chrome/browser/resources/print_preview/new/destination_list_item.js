// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-list-item',

  properties: {
    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @type {?RegExp} */
    searchQuery: Object,

    /** @private */
    stale_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private {string} */
    searchHint_: String,
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
    this.stale_ = this.destination.isOfflineOrInvalid;
  },

  update: function() {
    this.updateSearchHint_();
    this.updateHighlighting_();
  },

  /** @private */
  updateSearchHint_: function() {
    this.searchHint_ = !this.searchQuery ?
        '' :
        this.destination.extraPropertiesToMatch
            .filter(p => p.match(this.searchQuery))
            .join(' ');
  },

  /** @private */
  updateHighlighting_: function() {
    if (this.highlighted_) {
      cr.search_highlight_utils.findAndRemoveHighlights(this);
      this.highlighted_ = false;
    }

    if (!this.searchQuery)
      return;

    this.shadowRoot.querySelectorAll('.searchable').forEach(element => {
      element.childNodes.forEach(node => {
        if (node.nodeType != Node.TEXT_NODE)
          return;

        const textContent = node.nodeValue.trim();
        if (textContent.length == 0)
          return;

        if (this.searchQuery.test(textContent)) {
          // Don't highlight <select> nodes, yellow rectangles can't be
          // displayed within an <option>.
          // TODO(rbpotter): solve issue below before adding advanced
          // settings so that this function can be re-used.
          // TODO(dpapad): highlight <select> controls with a search bubble
          // instead.
          if (node.parentNode.nodeName != 'OPTION') {
            cr.search_highlight_utils.highlight(
                node, textContent.split(this.searchQuery));
            this.highlighted_ = true;
          }
        }
      });
    });
  },
});
