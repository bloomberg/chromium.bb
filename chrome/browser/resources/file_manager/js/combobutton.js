// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a combobutton control.
 */

cr.define('cr.ui', function() {
  /**
   * Sets minWidth for the target, so it's visually as large as source.
   * @param {HTMLElement} target Element which min-width to tune.
   * @param {HTMLElement} source Element, which width to use.
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
      if (this.items_.length > 0)
        // Remove default combobox item if we have added it at addItem.
        this.removeChild(this.firstChild);

      this.items_ = [];
      this.popup_.textContent = '';
      this.multiple = false;
      this.popup_.style.minWidth = '';
    },

    addItem: function(item) {
      this.items_.push(item);
      if (this.items_.length == 1) {
        // Set first added item as default on combobox.
        // First item should be the first element to prepend drop-down arrow and
        // popup layer.
        this.insertBefore(item, this.firstChild);
      } else {
        this.multiple = true;
        if (this.popup_.hasChildNodes())
          this.popup_.insertBefore(item, this.popup_.firstChild);
        else
          this.popup_.appendChild(item);
        if (this.visible)
          this.setPopupSize_();
      }
    },

    setPopupSize_: function() {
      this.popup_.style.bottom = (this.clientHeight + 1) + 'px';
      enlarge(this.popup_, this);
    },

    /**
     * Initializes the element.
     */
    decorate: function() {
      this.items_ = [];

      this.classList.add('combobutton');

      var triggerIcon = this.ownerDocument.createElement('span');
      triggerIcon.className = 'disclosureindicator';
      this.trigger_ = this.ownerDocument.createElement('div');
      this.trigger_.appendChild(triggerIcon);

      this.popup_ = this.ownerDocument.createElement('div');
      this.popup_.className = 'popup';

      this.appendChild(this.trigger_);
      this.appendChild(this.popup_);

      this.addEventListener('click',
          this.handleButtonClick_.bind(this));
      this.popup_.addEventListener('click',
          this.handlePopupClick_.bind(this));
      this.trigger_.addEventListener('click',
          this.handleTriggerClicked_.bind(this));
      this.addEventListener('mouseout', this.handleMouseOut_.bind(this));

      this.visible = true;
    },

    handleTriggerClicked_: function(event) {
      this.open = !this.open;
      event.stopPropagation();
    },

    handleMouseOut_: function(event) {
      var x = event.x;
      var y = event.y;

      var children = this.childNodes;
      for (var i = 0; i < children.length; i++)
      {
        var r = this.children[i].getBoundingClientRect();
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom)
          return;
      }

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
      event.stopPropagation();
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
        setTimeout(this.setPopupSize_.bind(this), 0);
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
  };
});
