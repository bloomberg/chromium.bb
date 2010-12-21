// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.browser_options', function() {
  const DeletableItemList = options.DeletableItemList;
  const DeletableItem = options.DeletableItem;

  /**
   * Creates a new startup page list item.
   * @param {Object} pageInfo The page this item represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function StartupPageListItem(pageInfo) {
    var el = cr.doc.createElement('div');
    el.pageInfo_ = pageInfo;
    StartupPageListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a startup page list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  StartupPageListItem.decorate = function(el) {
    el.__proto__ = StartupPageListItem.prototype;
    el.decorate();
  };

  StartupPageListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var titleEl = this.ownerDocument.createElement('span');
      titleEl.className = 'title';
      titleEl.classList.add('favicon-cell');
      titleEl.textContent = this.pageInfo_['title'];
      titleEl.style.backgroundImage = url('chrome://favicon/' +
                                          this.pageInfo_['url']);
      titleEl.title = this.pageInfo_['tooltip'];

      this.contentElement.appendChild(titleEl);
    },
  };

  var StartupPageList = cr.ui.define('list');

  StartupPageList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    createItem: function(pageInfo) {
      return new StartupPageListItem(pageInfo);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      chrome.send('removeStartupPages', [String(index)]);
    },
  };

  return {
    StartupPageList: StartupPageList
  };
});
