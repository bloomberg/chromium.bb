// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-search-engine-entry' is a component for showing a search
 * engine with its name, domain and query URL.
 *
 * @group Chrome Settings Elements
 * @element cr-search-engine-entry
 */
Polymer({
  is: 'cr-search-engine-entry',

  properties: {
    /** @type {!SearchEngine} */
    engine: Object
  },

  /** @private */
  deleteEngine_: function() {
    chrome.searchEnginesPrivate.removeSearchEngine(this.engine.guid);
  },

  /** @private */
  makeDefault_: function() {
    chrome.searchEnginesPrivate.setSelectedSearchEngine(this.engine.guid);
    this.makeNotEditable_();
  },

  /** @private */
  toggleEditable_: function() {
    this.$.domainField.disabled = !this.$.domainField.disabled;
    this.$.keywordField.disabled = !this.$.keywordField.disabled;
    this.$.queryURLField.disabled = !this.$.queryURLField.disabled;

    this.$.checkIcon.hidden =
        !this.$.checkIcon.hidden || this.engine.isSelected;
    this.$.deleteIcon.hidden =
        !this.$.deleteIcon.hidden || this.engine.isSelected;
  },

  /** @private */
  makeNotEditable_: function() {
    this.$.domainField.disabled = true;
    this.$.keywordField.disabled = true;
    this.$.queryURLField.disabled = true;

    this.$.checkIcon.hidden = true;
    this.$.deleteIcon.hidden = true;
  },

  /** @private */
  fieldChanged_: function() {
    // NOTE: This currently doesn't fire in response to a change event from the
    // paper-input, even though it should. This Polymer change should fix the
    // issue:  https://github.com/PolymerElements/paper-input/pull/33
    chrome.searchEnginesPrivate.updateSearchEngine(
        this.engine.guid,
        this.$.domainField.value,
        this.$.keywordField.value,
        this.$.queryURLField.value);
    this.makeNotEditable_();
  }
});
