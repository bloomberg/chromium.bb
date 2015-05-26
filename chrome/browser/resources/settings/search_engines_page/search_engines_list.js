// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-search-engines-list' is a component for showing a list of
 * search engines.
 *
 * @group Chrome Settings Elements
 * @element cr-search-engines-list
 */
Polymer({
  is: 'cr-search-engines-list',

  properties: {
    /** @type {!Array<!SearchEngine>} */
    engines: {
      type: Array,
      value: function() { return []; }
    }
  },
});
