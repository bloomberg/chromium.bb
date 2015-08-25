// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-error-screen-legacy',
  properties: {
    text: String
  },

  show: function() {
    this.style.visibility = 'visible';
  }
});
