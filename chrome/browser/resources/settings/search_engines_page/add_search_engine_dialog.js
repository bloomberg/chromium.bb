// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-add-search-engine-dialog' is a component for adding a
 * new search engine.
 *
 * @group Chrome Settings Elements
 * @element settings-add-search-engine-dialog
 */
Polymer({
  is: 'settings-add-search-engine-dialog',

  open: function() {
    this.$.dialog.open();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onAddTap_: function() {
    if (this.$.searchEngine.isInvalid ||
        this.$.keyword.isInvalid ||
        this.$.queryUrl.isInvalid) {
      // TODO(dpapad): Handle validation properly.
      this.$.dialog.close();
      return;
    }

    chrome.searchEnginesPrivate.addOtherSearchEngine(
        this.$.searchEngine.value, this.$.keyword.value,
        this.$.queryUrl.value);
    this.$.dialog.close();
  },
});
