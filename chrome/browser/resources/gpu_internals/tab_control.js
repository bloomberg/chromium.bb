// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview This implements a tab control.
 *
 * An individual tab within a tab control is, unsurprisingly, a Tab.
 * Tabs must be explicitly added/removed from the control.
 *
 * Tab titles are based on the label attribute of each child:
 *
 * <div>
 * <div label='Tab 1'>Hello</div>
 * <div label='Tab 2'>World</div>
 * </div>
 *
 * Results in:
 *
 * ---------------
 * | Tab1 | Tab2 |
 * | ---------------------------------------
 * | Hello World |
 * -----------------------------------------
 *
 */
cr.define('gpu', function() {
  /**
   * Creates a new tab element. A tab element is one of multiple tabs
   * within a TabControl.
   * @constructor
   * @param {Object=} opt_propertyBag Optional properties.
   * @extends {HTMLDivElement}
   */
  var Tab = cr.ui.define('div');
  Tab.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
    }
  };

  /**
   * Title for the tab.
   * @type {String}
   */
  cr.defineProperty(Tab, 'label', cr.PropertyKind.ATTR);

  /**
   * Whether the item is selected.
   * @type {boolean}
   */
  cr.defineProperty(Tab, 'selected', cr.PropertyKind.BOOL_ATTR);


  /**
   * Creates a new tab button element in the tabstrip
   * @constructor
   * @param {Object=} opt_propertyBag Optional properties.
   * @extends {HTMLDivElement}
   */
  var TabButton = cr.ui.define('a');
  TabButton.prototype = {
    __proto__: HTMLAnchorElement.prototype,

    decorate: function() {
      this.classList.add('tab-button');
      this.onclick = function() {
        if (this.tab_)
          this.parentNode.parentNode.selectedTab = this.tab_;
      }.bind(this);
    },
    get tab() {
      return this.tab_;
    },
    set tab(tab) {
      if (this.tab_)
        throw Error('Cannot set tab once set.');
      this.tab_ = tab;
      this.tab_.addEventListener('titleChange', this.onTabChanged_.bind(this));
      this.tab_.addEventListener('selectedChange',
                                 this.onTabChanged_.bind(this));
      this.onTabChanged_();
    },

    onTabChanged_: function(e) {
      if (this.tab_) {
        this.textContent = this.tab_.label;
        this.selected = this.tab_.selected;
      }
    }

  };

  /**
   * Whether the TabButton is selected.
   * @type {boolean}
   */
  cr.defineProperty(TabButton, 'selected', cr.PropertyKind.BOOL_ATTR);


  /**
   * Creates a new tab control element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var TabControl = cr.ui.define('div');
  TabControl.prototype = {
    __proto__: HTMLDivElement.prototype,

    selectedTab_: null,

    /**
     * Initializes the tab control element.
     * Any child elements pre-existing on the element will become tabs.
     */
    decorate: function() {
      this.classList.add('tab-control');

      this.tabStrip_ = this.ownerDocument.createElement('div');
      this.tabStrip_.classList.add('tab-strip');

      this.tabs_ = this.ownerDocument.createElement('div');
      this.tabs_.classList.add('tabs');

      this.insertBefore(this.tabs_, this.firstChild);
      this.insertBefore(this.tabStrip_, this.firstChild);

      this.boundOnTabSelectedChange_ = this.onTabSelectedChange_.bind(this);

      // Reparent existing tabs to the tabs_ div.
      while (this.children.length > 2)
        this.addTab(this.children[2]);
    },

    /**
     * Adds an element to the tab control.
     */
    addTab: function(tab) {
      if (tab.parentNode == this.tabs_)
        throw Error('Tab is already part of this control.');
      if (!(tab instanceof Tab))
        throw Error('Provided element is not instanceof Tab.');
      this.tabs_.appendChild(tab);

      tab.addEventListener('selectedChange', this.boundOnTabSelectedChange_);

      var button = new TabButton();
      button.tab = tab;
      tab.tabStripButton_ = button;

      this.tabStrip_.appendChild(button);

      if (this.tabs_.length == 1)
        this.tabs_.children[0].selected = true;
    },

    /**
     * Removes a tab from the tab control.
     * changing the selected tab if needed.
     */
    removeTab: function(tab) {
      if (tab.parentNode != this.tabs_)
        throw new Error('Tab is not attached to this control.');

      tab.removeEventListener('selectedChange', this.boundOnTabSelectedChange_);

      if (this.selectedTab_ == tab) {
        if (this.tabs_.children.length) {
          this.tabs_.children[0].selected = true;
        } else {
          this.selectedTab_ = undefined;
        }
      }

      this.tabs_.removeChild(tab);
      tab.tabStripButton_.parentNode.removeChild(
          tab.tabStripButton_);
    },

    /**
     * Gets the currently selected tab element.
     */
    get selectedTab() {
      return this.selectedTab_;
    },

    /**
     * Sets the currently selected tab element.
     */
    set selectedTab(tab) {
      if (tab.parentNode != this.tabs_)
        throw Error('Tab is not part of this TabControl.');
      tab.selected = true;
    },

    /**
     * Hides the previously selected tab element and dispatches a
     * 'selectedTabChanged' event.
     */
    onTabSelectedChange_: function(e) {
      var tab = e.target;
      if (!e.newValue) {
        // Usually we can ignore this event, as the tab becoming unselected
        // needs no corrective action. However, if the currently selected
        // tab is deselected, we do need to do some work.
        if (tab == this.selectedTab_) {
          var previousTab = this.selectedTab_;
          var newTab;
          for (var i = 0; i < this.tabs_.children.length; ++i) {
            if (this.tabs_.children[i] != tab) {
              newTab = this.tabs_.children[i];
              break;
            }
          }
          if (newTab) {
            newTab.selected = true;
          } else {
            this.selectedTab_ = undefined;
            cr.dispatchPropertyChange(
                this, 'selectedTab', this.selectedTab_, previousTab);
          }
        }
      } else {
        var previousTab = this.selectedTab_;
        this.selectedTab_ = tab;
        if (previousTab)
          previousTab.selected = false;
        cr.dispatchPropertyChange(
            this, 'selectedTab', this.selectedTab_, previousTab);
      }
    },

    /**
     * Returns an array of all the tabs within this control.  This is
     * not the same as this.children because the actual tab elements are
     * attached to the tabs_ element.
     */
    get tabs() {
      return Array.prototype.slice.call(this.tabs_.children);
    }
  };

  return {
    Tab: Tab,
    TabControl: TabControl
  };
});
