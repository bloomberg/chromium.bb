// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TabSwitcherView = (function() {
  'use strict';

  // We inherit from View.
  var superClass = View;

  /**
   * TabSwitcherView is an implementation of View that handles tab switching.
   *
   * It is comprised of many views (tabs), only one of which is visible at a
   * time.
   *
   * This view represents solely the selected tab's content area -- a separate
   * view needs to be maintained for the tab handles.
   *
   * @constructor
   */
  function TabSwitcherView() {
    superClass.call(this);
    this.tabs_ = [];
  }

  TabSwitcherView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    setGeometry: function(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);

      // Position each of the tabs content areas.
      for (var i = 0; i < this.tabs_.length; ++i) {
        var tab = this.tabs_[i];
        tab.contentView.setGeometry(left, top, width, height);
      }
    },

    show: function(isVisible) {
      superClass.prototype.show.call(this, isVisible);

      var activeTab = this.findActiveTab();
      if (activeTab)
        activeTab.contentView.show(isVisible);
    },

    /**
     * Adds a new tab (initially hidden).
     *
     * @param {string} id The ID for DOM node that will be made clickable to
     *                    select this tab. This is also the ID we use to
     *                    identify the "tab".
     * @param {!View} view The tab's actual contents.
     */
    addTab: function(id, contentView, switchOnClick, visible) {
      var tab = new TabEntry(id, contentView);
      this.tabs_.push(tab);

      if (switchOnClick) {
        // Attach a click handler, used to switch to the tab.
        var self = this;
        tab.getTabHandleNode().onclick = function() {
          self.switchToTab(id, null);
        };
      }

      // Start tabs off as hidden.
      tab.contentView.show(false);

      this.showTabHandleNode(id, visible);
    },

    /**
     * @return {?TabEntry} The currently selected tab, or null if there is none.
     */
    findActiveTab: function() {
      for (var i = 0; i < this.tabs_.length; ++i) {
        var tab = this.tabs_[i];
        if (tab.active)
          return tab;
      }
      return null;
    },

    /**
     * @return {?TabEntry} The tab with ID |id|, or null if there is none.
     */
    findTabById: function(id) {
      for (var i = 0; i < this.tabs_.length; ++i) {
        var tab = this.tabs_[i];
        if (tab.id == id)
          return tab;
      }
      return null;
    },

    /**
     * Focuses on tab with ID |id|.  |params| is a dictionary that will be
     * passed to the tab's setParameters function, if it's non-null.
     */
    switchToTab: function(id, params) {
      var oldTab = this.findActiveTab();
      if (oldTab)
        oldTab.setSelected(false);

      var newTab = this.findTabById(id);
      newTab.setSelected(true);
      if (params)
        newTab.contentView.setParameters(params);

      // Update data needed by newly active tab, as it may be
      // significantly out of date.
      if (typeof g_browser != 'undefined' && g_browser.checkForUpdatedInfo)
        g_browser.checkForUpdatedInfo();
    },

    getAllTabIds: function() {
      var ids = [];
      for (var i = 0; i < this.tabs_.length; ++i)
        ids.push(this.tabs_[i].id);
      return ids;
    },

    /**
     * Shows/hides the DOM node that is used to select the tab.  If the
     * specified tab is the active tab, switches the active tab to the first
     * still visible tab in the tab list.
     */
    showTabHandleNode: function(id, isVisible) {
      var tab = this.findTabById(id);
      if (!isVisible && tab == this.findActiveTab()) {
        for (var i = 0; i < this.tabs_.length; ++i) {
          if (this.tabs_[i].id != id &&
              this.tabs_[i].getTabHandleNode().style.display != 'none') {
            this.switchToTab(this.tabs_[i].id, null);
            break;
          }
        }
      }
      setNodeDisplay(tab.getTabHandleNode(), isVisible);
    }
  };

  //---------------------------------------------------------------------------

  /**
   * @constructor
   */
  function TabEntry(id, contentView) {
    this.id = id;
    this.contentView = contentView;
  }

  TabEntry.prototype.setSelected = function(isSelected) {
    this.active = isSelected;
    if (isSelected)
      this.getTabHandleNode().classList.add('selected');
    else
      this.getTabHandleNode().classList.remove('selected');
    this.contentView.show(isSelected);
  };

  /**
   * Returns the DOM node that is used to select the tab.
   */
  TabEntry.prototype.getTabHandleNode = function() {
    return $(this.id);
  };

  return TabSwitcherView;
})();
