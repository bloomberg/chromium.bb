// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;

  /**
   * Creates a new list item for the origin's data.
   * @param {!Object} origin Data used to create the origin list item.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function OriginListItem(origin) {
    var el = cr.doc.createElement('div');
    el.origin_ = origin.origin;
    el.usage_ = origin.usage;
    el.usageString_ = origin.usageString;
    el.readableName_ = origin.readableName;
    el.__proto__ = OriginListItem.prototype;
    el.decorate();
    return el;
  }

  OriginListItem.prototype = {
    __proto__: ListItem.prototype,

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      this.className = 'deletable-item origin-list-item';
      this.contentElement_ = this.ownerDocument.createElement('div');
      this.appendChild(this.contentElement_);

      var titleEl = this.ownerDocument.createElement('div');
      titleEl.className = 'title favicon-cell weaktrl';
      titleEl.textContent = this.readableName_;
      titleEl.originPattern = this.origin_;
      titleEl.style.backgroundImage = getFaviconImageSet(this.origin_);
      this.contentElement_.appendChild(titleEl);

      this.contentElement_.onclick = function() {
        chrome.send('maybeShowEditPage', [titleEl.originPattern]);
      };

      if (this.usageString_) {
        var usageEl = this.ownerDocument.createElement('span');
        usageEl.className = 'local-storage-usage';
        usageEl.textContent = this.usageString_;
        this.appendChild(usageEl);
      }
    }
  };

  /**
   * @constructor
   * @extends {cr.ui.List}
   */
  var OriginList = cr.ui.define('list');

  OriginList.prototype = {
    __proto__: List.prototype,

    /**
     * @override
     * @param {!Object} entry
     */
    createItem: function(entry) {
      return new OriginListItem(entry);
    },
  };

  return {
    OriginListItem: OriginListItem,
    OriginList: OriginList,
  };
});
