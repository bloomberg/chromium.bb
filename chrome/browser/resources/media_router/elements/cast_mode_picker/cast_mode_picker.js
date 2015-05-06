// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element is a drop down that shows the currently available
// cast modes.
Polymer('cast-mode-picker', {
  publish: {
    /**
     * The list of CastModes to show.
     *
     * @attribute castModeList
     * @type {?Array<!media_router.CastMode>}
     * @default null
     */
    castModeList: null,

    /**
     * The value of the selected cast mode in |castModeList|, or -1 if the
     * user has not explicitly selected a mode.
     *
     * @attribute selectedCastModeValue
     * @type {number}
     * @default -1
     */
    selectedCastModeValue: -1,
  },

  observe: {
    castModeList: 'updateSelectBoxSize',
  },

  created: function() {
    this.castModeList = [];
  },

  /**
   * Fires a cast-mode-click event. This is called when one of the cast modes
   * has been selected.
   *
   * @param {!Event} event The event object.
   * @param {!Object} detail The details of the event.
   * @param {!Element} sender Reference to clicked node.
   */
  onCastModeSelected: function(event, detail, sender) {
    this.castModeList.forEach(function(castMode) {
      if (castMode.type == sender.value) {
        this.fire('cast-mode-click', {
          headerText: this.castModeMap_[castMode.type].title
        });
        this.selectedCastModeValue = sender.value;
        return;
      }
    }, this);
  },

  /**
   * Called when |castModeList| has changed. Updates the size of the select
   * node.
   *
   * @param {?Array<!media_router.CastMode>} newValue The new value for
   *   |castModeList|.
   */
  updateSelectBoxSize: function(oldValue, newValue) {
    this.$.selectbox.size = Math.max(newValue.length, 1);
  },
});
