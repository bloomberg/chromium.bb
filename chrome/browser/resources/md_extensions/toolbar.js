// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  var Toolbar = Polymer({
    is: 'extensions-toolbar',

    /** @param {SearchFieldDelegate} delegate */
    setSearchDelegate: function(delegate) {
      this.$['search-field'].setDelegate(delegate);
    },
  });

  return {Toolbar: Toolbar};
});
