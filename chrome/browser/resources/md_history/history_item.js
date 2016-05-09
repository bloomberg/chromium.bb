// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-item',

  properties: {
    // Underlying HistoryEntry data for this item. Contains read-only fields
    // from the history backend, as well as fields computed by history-list.
    item: {
      type: Object,
      observer: 'showIcon_'
    },

    // True if the website is a bookmarked page.
    starred: {
      type: Boolean,
      reflectToAttribute: true
    },

    // Search term used to obtain this history-item.
    searchTerm: {
      type: String
    },

    selected: {
      type: Boolean,
      notify: true
    },

    isCardStart: {
      type: Boolean,
      reflectToAttribute: true
    },

    isCardEnd: {
      type: Boolean,
      reflectToAttribute: true
    },

    hasTimeGap: {
      type: Boolean
    },

    numberOfItems: {
      type: Number
    }
  },

  observers: [
    'setSearchedTextToBold_(item.title, searchTerm)'
  ],

  /**
   * When a history-item is selected the toolbar is notified and increases
   * or decreases its count of selected items accordingly.
   * @private
   */
  onCheckboxSelected_: function() {
    this.fire('history-checkbox-select', {
      countAddition: this.$.checkbox.checked ? 1 : -1
    });
  },

  /**
   * Fires a custom event when the menu button is clicked. Sends the details of
   * the history item and where the menu should appear.
   */
  onMenuButtonTap_: function(e) {
    this.fire('toggle-menu', {
      target: Polymer.dom(e).localTarget,
      itemIdentifier: {
        url: this.item.url,
        timestamps: this.item.time,
        domain: this.item.domain
      },
    });

    // Stops the 'tap' event from closing the menu when it opens.
    e.stopPropagation();
  },

  /**
   * Set the favicon image, based on the URL of the history item.
   * @private
   */
  showIcon_: function() {
    this.$.icon.style.backgroundImage =
        cr.icon.getFaviconImageSet(this.item.url);
  },

  /**
   * Updates the page title. If the result was from a search, highlights any
   * occurrences of the search term in bold.
   * @private
   */
  setSearchedTextToBold_: function() {
    var i = 0;
    var titleElem = this.$.title;
    var titleText = this.item.title;

    if (this.searchTerm == '' || this.searchTerm == null) {
      titleElem.textContent = titleText;
      return;
    }

    var re = new RegExp(quoteString(this.searchTerm), 'gim');
    var match;
    titleElem.textContent = '';
    while (match = re.exec(titleText)) {
      if (match.index > i)
        titleElem.appendChild(document.createTextNode(
            titleText.slice(i, match.index)));
      i = re.lastIndex;
      // Mark the highlighted text in bold.
      var b = document.createElement('b');
      b.textContent = titleText.substring(match.index, i);
      titleElem.appendChild(b);
    }
    if (i < titleText.length)
      titleElem.appendChild(
          document.createTextNode(titleText.slice(i)));
  },

  selectionNotAllowed_: function() {
    return !loadTimeData.getBoolean('allowDeletingHistory');
  },

  /**
   * Generates the title for this history card.
   * @param {number} numberOfItems The number of items in the card.
   * @param {string} search The search term associated with these results.
   * @private
   */
  cardTitle_: function(numberOfItems, historyDate, search) {
    if (!search)
      return this.item.dateRelativeDay;

    var resultId = numberOfItems == 1 ? 'searchResult' : 'searchResults';
    return loadTimeData.getStringF('foundSearchResults', numberOfItems,
        loadTimeData.getString(resultId), search);
  }
});
