// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @polymer */
Polymer({
  is: 'start-setup-page',

  properties: {
    /**
     * Overriden from ButtonNavigationBehavior
     */
    forwardButtonText: {
      type: String,
      value: 'Accept',
    },

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
