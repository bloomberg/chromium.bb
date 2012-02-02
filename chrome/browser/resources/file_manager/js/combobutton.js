// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a combobutton control.
 */

cr.define('cr.ui', function() {
  /**
   * Sets minWidth for the target, so it's visually as large as source.
   * @param {HTMLElement} target
   * @param {HTMLElement} source
   */
  function enlarge(target, source) {
    var cs = target.ownerDocument.defaultView.getComputedStyle(target);
    target.style.minWidth = (source.getBoundingClientRect().width -
        parseFloat(cs.borderLeftWidth) -
        parseFloat(cs.borderRightWidth)) + 'px';
  }

  /**
   * Creates a new combobutton element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLUListElement}
   */
  var ComboButton = cr.ui.define('div');

  ComboButton.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * The items list.
     */
    items_: null,

    clear: function() {
      this.items_ = [];
      this.popup_.textContent = '';
      this.buttonContainer_.textContent = '';
      this.multiple = false;
      this.style.minWidth = '0';
      this.popup_.style.minWidth = '0';
    },

    addItem: function(item) {
      this.items_.push(item);
      if (this.items_.length == 1) {
        this.buttonContainer_.appendChild(item);
      } else {
        this.multiple = true;
        if (this.visible)
          this.setPopupSize_();
        if (this.popup_.hasChildNodes())
          this.popup_.insertBefore(item, this.popup_.firstChild);
        else
          this.popup_.appendChild(item);
      }
    },

    setPopupSize_: function() {
      this.popup_.style.bottom = this.clientHeight + 'px';
      enlarge(this, this.popup_);
      enlarge(this.popup_, this);
    },

    /**
     * Initializes the element.
     */
    decorate: function() {
      this.items_ = [];

      this.classList.add('cr-combobutton');

      this.container_ = this.ownerDocument.createElement('div');
      this.container_.className = 'cr-cb-container';
      this.buttonContainer_ = this.ownerDocument.createElement('div');
      this.buttonContainer_.className = 'cr-button cr-cb-button-container';
      this.trigger_ = this.ownerDocument.createElement('div');
      this.trigger_.className = 'cr-button cr-cb-trigger';
      this.container_.appendChild(this.buttonContainer_);
      this.container_.appendChild(this.trigger_);

      this.popup_ = this.ownerDocument.createElement('div');
      this.popup_.className = 'cr-cb-popup';

      this.textContent = '';
      this.appendChild(this.container_);
      this.appendChild(this.popup_);

      this.buttonContainer_.addEventListener('click',
          this.handleButtonClick_.bind(this));
      this.popup_.addEventListener('click',
          this.handlePopupClick_.bind(this));
      this.trigger_.addEventListener('click',
          this.handleTriggerClicked_.bind(this));
      this.addEventListener('mouseout', this.handleMouseOut_.bind(this));
      this.popup_.addEventListener('mouseout',
          this.handleMouseOut_.bind(this));

      this.visible = true;
    },

    handleTriggerClicked_: function(event) {
      this.open = !this.open;
    },

    handleMouseOut_: function(event) {
      var x = event.x;
      var y = event.y;
      var r = this.popup_.getBoundingClientRect();
      if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom)
        return;
      r = this.container_.getBoundingClientRect();
      if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom)
        return;
      this.open = false;
    },

    handleButtonClick_: function(event) {
      this.dispatchSelectEvent(this.items_[0]);
    },

    handlePopupClick_: function(event) {
      var item = event.target;
      while (item && item.parentNode != this.popup_)
        item = item.parentNode;
      if (!item)
        return;

      this.open = false;
      this.dispatchSelectEvent(item);
    },

    dispatchSelectEvent: function(item) {
      var selectEvent = new Event('select');
      selectEvent.item = item;
      this.dispatchEvent(selectEvent);
    },

    get visible() {
      return this.hasAttribute('visible');
    },
    set visible(value) {
      if (value) {
        this.setAttribute('visible', 'visible');
        this.setPopupSize_();
      } else {
        this.removeAttribute('visible');
      }
    }
  };

  cr.defineProperty(ComboButton, 'disabled', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(ComboButton, 'open', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(ComboButton, 'multiple', cr.PropertyKind.BOOL_ATTR);

  return {
    ComboButton: ComboButton
  }
});
