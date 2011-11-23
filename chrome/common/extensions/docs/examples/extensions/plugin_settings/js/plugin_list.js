// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('pluginSettings.ui', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Returns the item's height, like offsetHeight but such that it works better
   * when the page is zoomed. See the similar calculation in @{code cr.ui.List}.
   * This version also accounts for the animation done in this file.
   * @param {Element} item The item to get the height of.
   * @return {number} The height of the item, calculated with zooming in mind.
   */
  function getItemHeight(item) {
    var height = item.style.height;
    // Use the fixed animation target height if set, in case the element is
    // currently being animated and we'd get an intermediate height below.
    if (height && height.substr(-2) == 'px')
      return parseInt(height.substr(0, height.length - 2));
    return item.getBoundingClientRect().height;
  }

  /**
   * Creates a new plug-in list item element.
   * @param {PluginList} list The plug-in list containing this item.
   * @param {Object} info Information about the plug-in.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function PluginListItem(list, info) {
    var el = cr.doc.createElement('li');
    el.list_ = list;
    el.info_ = info;
    el.__proto__ = PluginListItem.prototype;
    el.decorate();
    return el;
  }

  PluginListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * The plug-in list containing this item.
     * @type {PluginList}
     * @private
     */
    list_: null,

    /**
     * Information about the plug-in.
     * @type {Object}
     * @private
     */
    info_: null,

    /**
     * The element containing details about the plug-in.
     * @type {HTMLDivElemebt}
     * @private
     */
    detailsElement_: null,

    /**
     * Initializes the element.
     */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var info = this.info_;

      var contentElement = this.ownerDocument.createElement('div');

      var titleEl = this.ownerDocument.createElement('div');
      var nameEl = this.ownerDocument.createElement('span');
      nameEl.className = 'plugin-name';
      nameEl.textContent = info.description;
      nameEl.title = info.description;
      titleEl.appendChild(nameEl);
      this.numRulesEl_ = this.ownerDocument.createElement('span');
      this.numRulesEl_.className = 'num-rules';
      titleEl.appendChild(this.numRulesEl_);
      contentElement.appendChild(titleEl);

      this.detailsElement_ = this.ownerDocument.createElement('div');
      this.detailsElement_.className = 'plugin-details hidden';

      var columnHeadersEl = this.ownerDocument.createElement('div');
      columnHeadersEl.className = 'column-headers';
      var patternColumnEl = this.ownerDocument.createElement('div');
      patternColumnEl.textContent =
          chrome.i18n.getMessage("patternColumnHeader");
      patternColumnEl.className = 'pattern-column-header';
      var settingColumnEl = this.ownerDocument.createElement('div');
      settingColumnEl.textContent =
          chrome.i18n.getMessage("settingColumnHeader");
      settingColumnEl.className = 'setting-column-header';
      columnHeadersEl.appendChild(patternColumnEl);
      columnHeadersEl.appendChild(settingColumnEl);
      this.detailsElement_.appendChild(columnHeadersEl);
      contentElement.appendChild(this.detailsElement_);

      this.appendChild(contentElement);

      var settings = new pluginSettings.Settings(this.info_.id);
      this.updateRulesCount_(settings);
      settings.addEventListener('change',
                                this.updateRulesCount_.bind(this, settings));

      // Create the rule list asynchronously, to make sure that it is already
      // fully integrated in the DOM tree.
      window.setTimeout(this.loadRules_.bind(this, settings), 0);
    },

    /**
     * Create the list of content setting rules applying to this plug-in.
     * @private
     */
    loadRules_: function(settings) {
      var rulesEl = this.ownerDocument.createElement('list');
      this.detailsElement_.appendChild(rulesEl);

      pluginSettings.ui.RuleList.decorate(rulesEl);
      rulesEl.setPluginSettings(settings);
    },

    updateRulesCount_: function(settings) {
      this.numRulesEl_.textContent = '(' + settings.getAll().length + ' rules)';
    },

    /**
     * Whether this item is expanded or not.
     * @type {boolean}
     */
    expanded_: false,
    get expanded() {
      return this.expanded_;
    },
    set expanded(expanded) {
      if (this.expanded_ == expanded)
        return;
      this.expanded_ = expanded;
      if (expanded) {
        var oldExpanded = this.list_.expandItem;
        this.list_.expandItem = this;
        this.detailsElement_.classList.remove('hidden');
        if (oldExpanded)
          oldExpanded.expanded = false;
        this.classList.add('plugin-show-details');
      } else {
        if (this.list_.expandItem == this) {
          this.list_.leadItemHeight = 0;
          this.list_.expandItem = null;
        }
        this.style.height = '';
        this.detailsElement_.classList.add('hidden');
        this.classList.remove('plugin-show-details');
      }
    },
  };

  /**
   * Creates a new plug-in list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var PluginList = cr.ui.define('list');

  PluginList.prototype = {
    __proto__: List.prototype,

    /**
     * Initializes the element.
     */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.classList.add('plugin-list');
      var sm = new ListSingleSelectionModel();
      sm.addEventListener('change', this.handleSelectionChange_.bind(this));
      this.selectionModel = sm;
      this.autoExpands = true;
    },

    /**
     * Creates a new plug-in list item.
     * @param {Object} info Information about the plug-in.
     */
    createItem: function(info) {
      return new PluginListItem(this, info);
    },

    /**
     * Called when the selection changes.
     * @private
     */
    handleSelectionChange_: function(ce) {
      ce.changes.forEach(function(change) {
        var listItem = this.getListItemByIndex(change.index);
        if (listItem) {
          if (!change.selected) {
            // TODO(bsmith) explain window timeout (from cookies_list.js)
            window.setTimeout(function() {
              if (!listItem.selected || !listItem.lead)
                listItem.expanded = false;
            }, 0);
          } else if (listItem.lead) {
            listItem.expanded = true;
          }
        }
      }, this);
    },
  };

  return {
    PluginList: PluginList,
    PluginListItem: PluginListItem,
  };
});
