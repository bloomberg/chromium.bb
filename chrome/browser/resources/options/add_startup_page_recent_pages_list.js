// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.add_startup_page', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;

  /**
   * Creates a new search engine list item.
   * @param {Object} searchEnigne The search engine this represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function RecentPageListItem(pageInfo) {
    var el = cr.doc.createElement('div');
    el.pageInfo = pageInfo;
    RecentPageListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a search engine list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  RecentPageListItem.decorate = function(el) {
    el.__proto__ = RecentPageListItem.prototype;
    el.decorate();
  };

  RecentPageListItem.prototype = {
    __proto__: ListItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var titleEl = this.ownerDocument.createElement('span');
      titleEl.className = 'title';
      titleEl.textContent = this.pageInfo['title'];
      titleEl.style.backgroundImage = url('chrome://favicon/' +
                                         this.pageInfo['url']);

      var urlEL = this.ownerDocument.createElement('span');
      urlEL.className = 'url';
      urlEL.textContent = this.pageInfo['displayURL'];

      this.appendChild(titleEl);
      this.appendChild(urlEL);
    },
  };

  var RecentPageList = cr.ui.define('list');

  RecentPageList.prototype = {
    __proto__: List.prototype,

    /** @inheritDoc */
    createItem: function(pageInfo) {
      return new RecentPageListItem(pageInfo);
    },
  };

  return {
    RecentPageList: RecentPageList
  };
});
