// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ List = cr.ui.List;
  /** @const */ ListItem = cr.ui.ListItem;

  function OriginListItem(origin) {
    var el = cr.doc.createElement('div');
    el.origin_ = origin;
    el.__proto__ = OriginListItem.prototype;
    el.decorate();
    return el;
  }

  OriginListItem.prototype = {
    __proto__: ListItem.prototype,

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      this.classList.add('deletable-item');

      this.contentElement_ = this.ownerDocument.createElement('div');
      this.appendChild(this.contentElement_);

      var titleEl = this.ownerDocument.createElement('div');
      titleEl.className = 'title favicon-cell weaktrl';
      titleEl.textContent = this.origin_;
      titleEl.style.backgroundImage = getFaviconImageSet(this.origin_);
      this.contentElement_.appendChild(titleEl);
    }
  };

  var OriginList = cr.ui.define('list');

  OriginList.prototype = {
    __proto__: List.prototype,

    /** @override */
    createItem: function(entry) {
      return new OriginListItem(entry);
    },
  };

  return {
    OriginListItem: OriginListItem,
    OriginList: OriginList,
  };
});
