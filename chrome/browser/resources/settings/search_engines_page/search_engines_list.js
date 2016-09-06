// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-list' is a component for showing a
 * list of search engines.
 */
Polymer({
  is: 'settings-search-engines-list',

  properties: {
    /** @type {!Array<!SearchEngine>} */
    engines: {
      type: Array,
      value: function() { return []; },
      observer: 'enginesChanged_',
    }
  },

  /** @private */
  enginesChanged_: function() {
    // Rather than adding an event to notify this element when it is shown,
    // observe changes to |engines| and notify the iron-list.
    this.$$('iron-list').fire('iron-resize');
  },
});
