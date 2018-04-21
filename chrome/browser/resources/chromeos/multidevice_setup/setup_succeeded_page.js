// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'setup-succeeded-page',

  properties: {
    /**
     * Overriden from ButtonNavigationBehavior
     */
    forwardButtonTextId: {
      type: String,
      value: 'done',
    },
  },

  behaviors: [
    ButtonNavigationBehavior,
  ],
});
