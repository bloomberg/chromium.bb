// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'strict';

  /**
   * @constructor
   * This class represents a margin line and a textbox corresponding to that
   * margin.
   */
  function MarginsUIPair(groupName) {
    this.line_ = new print_preview.MarginLine(groupName);
    this.box_ = new print_preview.MarginTextbox(groupName);
  }

  MarginsUIPair.prototype = {
    __proto__: MarginsUIPair.prototype,

    /**
     * Updates the state.
     */
    update: function(marginsRectangle, value, valueLimit, keepDisplayedValue) {
      this.line_.update(marginsRectangle);
      this.box_.update(marginsRectangle, value, valueLimit, keepDisplayedValue);
    },

    /**
     * Draws |this| based on the state.
     */
    draw: function() {
      this.line_.draw();
      this.box_.draw();
    }
  };

  function MarginsUI(parentNode) {
    var marginsUI = document.createElement('div');
    marginsUI.__proto__ = MarginsUI.prototype;
    marginsUI.id = 'customized-margins';

    marginsUI.topPair_ = new MarginsUIPair(
        print_preview.MarginSettings.TOP_GROUP);
    marginsUI.leftPair_ = new MarginsUIPair(
        print_preview.MarginSettings.LEFT_GROUP);
    marginsUI.rightPair_ = new MarginsUIPair(
        print_preview.MarginSettings.RIGHT_GROUP);
    marginsUI.bottomPair_ = new MarginsUIPair(
        print_preview.MarginSettings.BOTTOM_GROUP);
    parentNode.appendChild(marginsUI);

    var uiPairs = marginsUI.pairsAsList;
    for (var i = 0; i < uiPairs.length; i++) {
      marginsUI.appendChild(uiPairs[i].line_);
      marginsUI.appendChild(uiPairs[i].box_);
    }
    return marginsUI;
  }

  MarginsUI.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Adds an observer for |MarginsMayHaveChanged| event.
     * @param {function} func A callback function to be called when
     *   |MarginsMayHaveChanged| event occurs.
     */
    addObserver: function(func) {
      var uiPairs = this.pairsAsList;
      for (var i = 0; i < uiPairs.length; i++)
        uiPairs[i].box_.addEventListener('MarginsMayHaveChanged', func);
    },

    /**
     * @return {array} An array including all |MarginUIPair| objects.
     */
    get pairsAsList() {
      return [this.topPair_, this.leftPair_, this.rightPair_, this.bottomPair_];
    },

    /**
     * Updates the state of the margins UI.
     * @param {print_preview.Rect}
     * @param {Margins} marginValues
     * @param {array} valueLimits
     */
    update: function(marginsRectangle, marginValues, valueLimits,
        keepDisplayedValue) {
      var uiPairs = this.pairsAsList;
      var order = ['top', 'left', 'right', 'bottom'];
      for (var i = 0; i < uiPairs.length; i++) {
        uiPairs[i].update(marginsRectangle,
                          marginValues[order[i]],
                          valueLimits[i],
                          keepDisplayedValue);
      }
    },

    /**
     * Draws |this| based on the latest state.
     */
    draw: function() {
      this.applyClippingMask_();
      this.pairsAsList.forEach(function(pair) { pair.draw(); });
    },

    /**
     * Shows the margins UI.
     */
    show: function() {
      this.hidden = false;
    },

    /**
     * Hides the margins UI.
     */
    hide: function() {
      this.hidden = true;
    },

    /**
     * Applies a clipping mask on |this| so that it does not paint on top of the
     * scrollbars (if any).
     */
    applyClippingMask_: function() {
      var bottom = previewArea.height;
      var right = previewArea.width;
      this.style.clip = "rect(0, " + right + "px, " + bottom + "px, 0)";
    }
  };

  return {
    MarginsUI: MarginsUI
  };
});
