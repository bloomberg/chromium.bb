// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  var Toolbar = Polymer({
    is: 'extensions-toolbar',

    /** @param {string} searchTerm */
    onSearchTermSearch: function(searchTerm) {
    },
  });

  /**
   * @constructor
   * @implements {SearchFieldDelegate}
   */
  // TODO(devlin): This is a bit excessive, and it would be better to just have
  // Toolbar implement SearchFieldDelegate. But for now, we don't know how to
  // make that happen with closure compiler.
  function ToolbarSearchFieldDelegate(toolbar) {
    this.toolbar_ = toolbar;
  }

  ToolbarSearchFieldDelegate.prototype = {
    /** @override */
    onSearchTermSearch: function(searchTerm) {
      this.toolbar_.onSearchTermSearch(searchTerm);
    }
  };

  return {Toolbar: Toolbar};
});
