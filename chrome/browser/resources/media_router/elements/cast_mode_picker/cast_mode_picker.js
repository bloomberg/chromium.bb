// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('cast-mode-picker', {
  publish: {
    /**
     * The list of CastModes to show.
     *
     * @attribute castModeList
     * @type {!Array<!media_router.CastMode>}
     * @default []
     */
    castModeList: [],

    /**
     * Maps each cast mode ID to the type of cast mode.
     *
     * @attribute castModeMap
     * @type {!Object<number, !media_router.CastMode>}
     * @default {}
    */
    castModeMap: {}
  },

  created: function() {
    this.castModeList = [];
    this.castModeMap = {};

    /**
     * -1 means users did not select mode explicitly.
     * @type {number}
     */
    this.selectedCastMode = -1;
  },

  castModeListChanged: function(oldValue, newValue) {
    this.$.selectbox.size = Math.max(newValue.length, 1);
    this.castModeMap = {};
    this.castModeList.forEach(function(e) {
      this.castModeMap[e.castMode] = e;
    }, this);
  },

  onCastModeSelected: function(event, detail, sender) {
    this.fire('cast-mode-click', {
      headerText: this.castModeMap[sender.value].title
    });
  }
});
