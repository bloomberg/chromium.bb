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
 * Shows a registered page.
 * @param {string} pageName Page name.
 */
OptionsPage.showPageByName = function(pageName) {
  for (var name in OptionsPage.registeredPages_) {
    var page = OptionsPage.registeredPages_[name];
    page.visible = name == pageName;
  }
};

OptionsPage.setState = function(state) {
  if (state.pageName)
    OptionsPage.showPageByName(state.pageName);
};

/**
 * Registers new options page.
 * @param {OptionsPage} page Page to register, must be of a class derived from
 * OptionsPage.
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

OptionsPage.prototype = {
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
      this.tab.classList.add('navbar-item-selected');
    } else {
      this.pageDiv.style.display = 'none';
      this.tab.classList.remove('navbar-item-selected');
    }
  }
};
