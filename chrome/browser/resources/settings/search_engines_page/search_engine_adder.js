// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-search-engine-adder' is a component for adding a new search
 * engine.
 *
 * @group Chrome Settings Elements
 * @element settings-search-engine-adder
 */
Polymer({
  is: 'cr-search-engine-adder',

  /** @private */
  add_: function() {
    if (!this.$.domainField.isInvalid &&
        !this.$.keywordField.isInvalid &&
        !this.$.queryURLField.isInvalid) {
      chrome.searchEnginesPrivate.addOtherSearchEngine(
          /* name */ this.$.domainField.value,
          /* keyword */ this.$.keywordField.value,
          /* url */ this.$.queryURLField.value);
      this.$.domainField.value = '';
      this.$.keywordField.value = '';
      this.$.queryURLField.value = '';
    }
  },
});
