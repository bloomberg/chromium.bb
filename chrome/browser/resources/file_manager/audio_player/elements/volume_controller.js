// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  Polymer('volume-controller', {
    /**
     * Initializes an element. This method is called automatically when the
     * element is ready.
     */
    ready: function() {
      this.style.width = this.width + 'px';
      this.style.height = this.height + 'px';

      this.$.rawValueInput.style.width = this.height + 'px';
      this.$.rawValueInput.style.height = this.width + 'px';
      this.$.rawValueInput.style.webkitTransformOrigin =
          (this.width / 2) + 'px ' +
          (this.width / 2 - 2) + 'px';

      var barLeft = (this.width / 2 - 1);
      this.$.bar.style.left = barLeft + 'px';
      this.$.bar.style.right = barLeft + 'px';
    },

    /**
     * Volume. 0 is silent, and 100 is maximum.
     * @type {number}
     */
    value: 50,

    /**
     * Volume. 1000 is silent, and 0 is maximum.
     * @type {number}
     */
    rawValue: 0,

    /**
     * Height of the element in pixels. Must be specified before ready() is
     * called. Dynamic change is not supprted.
     * @type {number}
     */
    height: 100,

    /**
     * Width of the element in pixels. Must be specified before ready() is
     * called. Dynamic change is not supported.
     * @type {number}
     */
    width: 32,

    /**
     * Invoked the 'value' property is changed.
     * @param {number} oldValue Old value.
     * @param {number} newValue New value.
     */
    valueChanged: function(oldValue, newValue) {
      if (oldValue != newValue)
        this.rawValue = 100 - newValue;
      this.fire('changed');
    },

    /**
     * Invoked the 'rawValue' property is changed.
     * @param {number} oldValue Old value.
     * @param {number} newValue New value.
     */
    rawValueChanged: function(oldValue, newValue) {
      if (oldValue != newValue)
        this.value = 100 - newValue;
    },
  });
})();  // Anonymous closure
