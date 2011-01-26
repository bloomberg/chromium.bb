// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TabSwitcher is an implementation of View that handles tab switching.
 *
 *  +-----------------------------------+
 *  | Tab1 / Tab2 / Tab3 / ..           |  <- tab handle view
 *  +-----------------------------------+
 *  |                                   |
 *  |                                   |
 *  |                                   |
 *  |   stacked tab content areas       |
 *  |   (only 1 is visible at a time)   |
 *  |                                   |
 *  |                                   |
 *  |                                   |
 *  +-----------------------------------+
 *
 * @parameter {!View} tabHandleView the view that contains the tab handles.
 *
 * @constructor
 */
function TabSwitcherView(tabHandleDivId) {
  document.getElementById(tabHandleDivId).classList.add('tab-switcher-view');
  var tabHandleView = new DivView(tabHandleDivId);

  View.call(this);
  this.tabHandleView_ = tabHandleView;
  this.tabs_ = [];
}

inherits(TabSwitcherView, View);

TabSwitcherView.prototype.setGeometry = function(left, top, width, height) {
  TabSwitcherView.superClass_.setGeometry.call(this, left, top, width, height);

  this.tabHandleView_.setGeometry(
      left, top, width, this.tabHandleView_.getHeight());

  var contentTop = this.tabHandleView_.getBottom();
  var contentHeight = height - this.tabHandleView_.getHeight();

  // Position each of the tabs content areas.
  for (var i = 0; i < this.tabs_.length; ++i) {
    var tab = this.tabs_[i];
    tab.contentView.setGeometry(left, contentTop, width, contentHeight);
  }
};

TabSwitcherView.prototype.show = function(isVisible) {
  TabSwitcherView.superClass_.show.call(this, isVisible);

  this.tabHandleView_.show(isVisible);

  var activeTab = this.findActiveTab();
  if (activeTab)
    activeTab.contentView.show(isVisible);
};

/**
 * Adds a new tab (initially hidden).
 *
 * @param {String} id The ID for DOM node that will be made clickable to select
 *                    this tab. This is also the ID we use to identify the
 *                    "tab".
 * @param {!View} view The tab's actual contents.
 */
TabSwitcherView.prototype.addTab = function(id, contentView, switchOnClick) {
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
};

/**
 * Returns the currently selected tab, or null if there is none.
 * @returns {!TabEntry}
 */
TabSwitcherView.prototype.findActiveTab = function() {
  for (var i = 0; i < this.tabs_.length; ++i) {
    var tab = this.tabs_[i];
    if (tab.active)
      return tab;
  }
  return null;
};

/**
 * Returns the tab with ID |id|.
 * @returns {!TabEntry}
 */
TabSwitcherView.prototype.findTabById = function(id) {
  for (var i = 0; i < this.tabs_.length; ++i) {
    var tab = this.tabs_[i];
    if (tab.id == id)
      return tab;
  }
  return null;
};

/**
 * Focuses on tab with ID |id|.  |params| is a dictionary that will be
 * passed to the tab's setParameters function, if it's non-null.
 */
TabSwitcherView.prototype.switchToTab = function(id, params) {
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
};

TabSwitcherView.prototype.getAllTabIds = function() {
  var ids = [];
  for (var i = 0; i < this.tabs_.length; ++i)
    ids.push(this.tabs_[i].id);
  return ids;
};

// Shows/hides the DOM node that is used to select the tab.  Will not change
// the active tab.
TabSwitcherView.prototype.showTabHandleNode = function(id, isVisible) {
  var tab = this.findTabById(id);
  setNodeDisplay(tab.getTabHandleNode(), isVisible);
};

//-----------------------------------------------------------------------------

/**
 * @constructor
 */
function TabEntry(id, contentView) {
  this.id = id;
  this.contentView = contentView;
}

TabEntry.prototype.setSelected = function(isSelected) {
  this.active = isSelected;
  changeClassName(this.getTabHandleNode(), 'selected', isSelected);
  this.contentView.show(isSelected);
};

/**
 * Returns the DOM node that is used to select the tab.
 */
TabEntry.prototype.getTabHandleNode = function() {
  return document.getElementById(this.id);
};

