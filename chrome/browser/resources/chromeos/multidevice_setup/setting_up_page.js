// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @polymer */
Polymer({
  is: 'setting-up-page',

  properties: {
    /**
     * Overriden from ButtonNavigationBehavior
     */
    backwardButtonText: {
      type: String,
      value: 'Cancel',
    },
  },

  behaviors: [
    ButtonNavigationBehavior,
  ],
});
