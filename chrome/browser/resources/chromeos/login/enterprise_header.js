// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'enterprise-header',

  properties: {
    /**
     * Title of the header
     * @type {String}
     */
    headerTitle: {type: String, value: ''},

    /**
     * Additional text shown in the header
     * @type {String}
     */
    headerComment: {type: String, value: ''},
  },
});
