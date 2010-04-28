// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {

  /**
   * Creates a new list item element.
   * @param {string} opt_label The text label for the item.
   * @constructor
   * @extends {HTMLLIElement}
   */
  var ListItem = cr.ui.define('li');

  ListItem.prototype = {
    __proto__: HTMLLIElement.prototype,

    /**
     * Plain text label.
     * @type {string}
     */
    get label() {
      return this.textContent;
    },
    set label(label) {
      this.textContent = label;
    },

    /**
     * Whether the item is the lead in a selection. Setting this does not update
     * the underlying selection model. This is only used for display purpose.
     * @type {boolean}
     */
    get lead() {
      return this.hasAttribute('lead');
    },
    set lead(lead) {
      if (lead) {
        this.setAttribute('lead', '');
        this.scrollIntoViewIfNeeded(false);
      } else {
        this.removeAttribute('lead');
      }
    },

    /**
     * Called when an element is decorated as a list item.
     */
    decorate: function() {
    }
  };

  cr.defineProperty(ListItem, 'selected', cr.PropertyKind.BOOL_ATTR);

  return {
    ListItem: ListItem
  };
});
