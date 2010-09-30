// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /////////////////////////////////////////////////////////////////////////////
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
    this.managed = false;
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
   * Called on load. Dispatch the URL hash to the given page's handleHash
   * function.
   * @param {string} pageName The string name of the (registered) options page.
   * @param {string} hash The value of the hash component of the URL.
   */
  OptionsPage.handleHashForPage = function(pageName, hash) {
    OptionsPage.registeredPages_[pageName].handleHash(hash);
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
  * Shows the tab contents for the given navigation tab.
  * @param {!Element} tab The tab that the user clicked.
  */
  OptionsPage.showTab = function(tab) {
    if (!tab.classList.contains('inactive-tab'))
      return;

    if (this.activeNavTab != null) {
      this.activeNavTab.classList.remove('active-tab');
      $(this.activeNavTab.getAttribute('tab-contents')).classList.
          remove('active-tab-contents');
    }

    tab.classList.add('active-tab');
    $(tab.getAttribute('tab-contents')).classList.add('active-tab-contents');
    this.activeNavTab = tab;
  }

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
    pageNav.tabIndex = 0;
    pageNav.onclick = function(event) {
      OptionsPage.showPageByName(this.getAttribute('pageName'));
    };
    pageNav.onkeypress = function(event) {
      // Enter or space
      if (event.keyCode == 13 || event.keyCode == 32) {
        OptionsPage.showPageByName(this.getAttribute('pageName'));
      }
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
   * @param {OptionsPage} page Page to register, must be a class derived from
   * OptionsPage.
   */
  OptionsPage.registerOverlay = function(page) {
    OptionsPage.registeredOverlayPages_[page.name] = page;
    page.tab = undefined;
    page.isOverlay = true;
    page.initializePage();
  };

  /**
   * Callback for window.onpopstate.
   * @param {Object} data State data pushed into history.
   */
  OptionsPage.setState = function(data) {
    if (data && data.pageName) {
      OptionsPage.showPageByName(data.pageName);
    }
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
     * Sets managed banner visibility state.
     */
    setManagedBannerVisibility: function(visible) {
      this.managed = visible;
      if (this.visible) {
        $('managed-prefs-banner').style.display = visible ? 'block' : 'none';
      }
    },

    /**
     * Gets page visibility state.
     */
    get visible() {
      var page = $(this.pageDivName);
      return page && page.ownerDocument.defaultView.getComputedStyle(
          page).display == 'block';
    },

    /**
     * Sets page visibility.
     */
    set visible(visible) {
      if ((this.visible && visible) || (!this.visible && !visible))
        return;

      if (visible) {
        this.pageDiv.style.display = 'block';
        if (this.isOverlay) {
          var overlay = $('overlay');
          overlay.classList.remove('hidden');
        } else {
          var banner = $('managed-prefs-banner');
          banner.style.display = this.managed ? 'block' : 'none';

          // Recent webkit change no longer allows url change from "chrome://".
          window.history.pushState({pageName: this.name},
                                   this.title);
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
    },

    /**
     * Handles a hash value in the URL (such as bar in
     * chrome://options/foo#bar). Called on page load. By default, this shows
     * an overlay that matches the hash name, but can be overriden by individual
     * OptionsPage subclasses to get other behavior.
     * @param {string} hash The hash value.
     */
    handleHash: function(hash) {
      OptionsPage.showOverlay(hash);
    },
  };

  // Export
  return {
    OptionsPage: OptionsPage
  };

});
