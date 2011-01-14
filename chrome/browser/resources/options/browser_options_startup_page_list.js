// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.browser_options', function() {
  const InlineEditableItem = options.InlineEditableItem;
  const InlineEditableItemList = options.InlineEditableItemList;

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
    __proto__: InlineEditableItem.prototype,

    /**
     * Input field for editing the page url.
     * @type {HTMLElement}
     * @private
     */
    urlField_: null,

    /** @inheritDoc */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      var titleEl = this.ownerDocument.createElement('div');
      titleEl.className = 'title';
      titleEl.classList.add('favicon-cell');
      titleEl.textContent = this.pageInfo_['title'];
      titleEl.style.backgroundImage = url('chrome://favicon/' +
                                          this.pageInfo_['url']);
      titleEl.title = this.pageInfo_['tooltip'];

      this.contentElement.appendChild(titleEl);

      var urlEl = this.createEditableTextCell(this.pageInfo_['url']);
      urlEl.className = 'url';
      this.contentElement.appendChild(urlEl);

      this.urlField_ = urlEl.querySelector('input');
      this.urlField_.required = true;

      this.addEventListener('commitedit', this.onEditCommitted_.bind(this));
    },

    /** @inheritDoc */
    get currentInputIsValid() {
      return this.urlField_.validity.valid;
    },

    /** @inheritDoc */
    get hasBeenEdited() {
      return this.urlField_.value != this.pageInfo_['url'];
    },

    /**
     * Called when committing an edit; updates the model.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      chrome.send('editStartupPage',
                  [this.pageInfo_['modelIndex'], this.urlField_.value]);
    },
  };

  var StartupPageList = cr.ui.define('list');

  StartupPageList.prototype = {
    __proto__: InlineEditableItemList.prototype,

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
