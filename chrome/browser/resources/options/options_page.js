// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// OptionsPage class:

/**
 * Base class for options page.
 * @constructor
 * @param {string} name Options page name, also defines id of the div element
 *     containing the options view and the name of options page navigation bar
 *     item as name+'PageNav'.
 * @param {string} title Options page title, used for navigation bar
 * @extends {EventTarget}
 */
function OptionsPage(name, title, pageDivName) {
  this.name = name;
  this.title = title;
  this.pageDivName = pageDivName;
  this.pageDiv = $(this.pageDivName);
  this.tab = null;
}

OptionsPage.registeredPages_ = {};

/**
 * Pages which are meant to have an entry in the nav, but
 * not have a permanent entry.
 */
OptionsPage.registeredSubPages_ = {};

/**
 * Pages which are meant to behave like model dialogs.
 */
OptionsPage.registeredOverlayPages_ = {};

/**
 * Shows a registered page.
 * @param {string} pageName Page name.
 */
OptionsPage.showPageByName = function(pageName) {
  for (var name in OptionsPage.registeredPages_) {
    var page = OptionsPage.registeredPages_[name];
    page.visible = name == pageName;
  }
};

/**
 * Shows a registered Overlay page.
 * @param {string} overlayName Page name.
 */
OptionsPage.showOverlay = function(overlayName) {
  if (OptionsPage.registeredOverlayPages_[overlayName]) {
    OptionsPage.registeredOverlayPages_[overlayName].visible = true;
  }
};

OptionsPage.clearOverlays = function() {
   for (var name in OptionsPage.registeredOverlayPages_) {
     var page = OptionsPage.registeredOverlayPages_[name];
     page.visible = false;
   }
};

/**
 * Registers new options page.
 * @param {OptionsPage} page Page to register.
 */
OptionsPage.register = function(page) {
  OptionsPage.registeredPages_[page.name] = page;
  // Create and add new page <li> element to navbar.
  var pageNav = document.createElement('li');
  pageNav.id = page.name + 'PageNav';
  pageNav.className = 'navbar-item';
  pageNav.setAttribute('pageName', page.name);
  pageNav.textContent = page.title;
  pageNav.onclick = function(event) {
    OptionsPage.showPageByName(this.getAttribute('pageName'));
  };
  var navbar = $('navbar');
  navbar.appendChild(pageNav);
  page.tab = pageNav;
  page.initializePage();
};

/**
 * Registers a new Sub tab page.
 * @param {OptionsPage} page Page to register.
 */
OptionsPage.registerSubPage = function(page) {
  OptionsPage.registeredPages_[page.name] = page;
  var pageNav = document.createElement('li');
  pageNav.id = page.name + 'PageNav';
  pageNav.className = 'navbar-item hidden';
  pageNav.setAttribute('pageName', page.name);
  pageNav.textContent = page.title;
  var subpagesnav = $('subpagesnav');
  subpagesnav.appendChild(pageNav);
  page.tab = pageNav;
  page.initializePage();
};

/**
 * Registers a new Overlay page.
 * @param {OptionsPage} page Page to register, must be a class derviced from
 * OptionsPage.
 */
OptionsPage.registerOverlay = function(page) {
  OptionsPage.registeredOverlayPages_[page.name] = page;
  page.tab = undefined;
  page.isOverlay = true;
  page.initializePage();
};

/**
 * Initializes the complete options page.  This will cause
 * all C++ handlers to be invoked to do final setup.
 */
OptionsPage.initialize = function() {
  chrome.send('coreOptionsInitialize');
};

OptionsPage.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Initializes page content.
   */
  initializePage: function() {},

  /**
   * Gets page visibility state.
   */
  get visible() {
    var page = $(this.pageDivName);
    return page.ownerDocument.defaultView.getComputedStyle(
        page).display == 'block';
  },

  /**
   * Sets page visibility.
   */
  set visible(visible) {
    if ((this.visible && visible) || (!this.visible && !visible))
      return;

    if (visible) {
      window.history.pushState({pageName: this.name},
                               this.title,
                               '/' + this.name);
      this.pageDiv.style.display = 'block';
      if (this.isOverlay) {
        var overlay = $('overlay');
        overlay.classList.remove('hidden');
      }
      if (this.tab) {
        this.tab.classList.add('navbar-item-selected');
        if (this.tab.parentNode && this.tab.parentNode.id == 'subpagesnav')
          this.tab.classList.remove('hidden');
      }
    } else {
      if (this.isOverlay) {
        var overlay = $('overlay');
        overlay.classList.add('hidden');
      }
      this.pageDiv.style.display = 'none';
      if (this.tab) {
        this.tab.classList.remove('navbar-item-selected');
        if (this.tab.parentNode && this.tab.parentNode.id == 'subpagesnav')
          this.tab.classList.add('hidden');
      }
    }

    cr.dispatchPropertyChange(this, 'visible', visible, !visible);
  }
};
