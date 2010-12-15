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

  /**
   * Main level option pages.
   * @protected
   */
  OptionsPage.registeredPages = {};

  /**
   * Pages which are nested under a main level page.
   * @protected
   */
  OptionsPage.registeredSubPages = {};

  /**
   * Pages which are meant to behave like modal dialogs.
   * @protected
   */
  OptionsPage.registeredOverlayPages = {};

  /**
   * Whether or not |initialize| has been called.
   * @private
   */
  OptionsPage.initialized_ = false;

  /**
   * Shows a registered page. This handles both top-level pages and sub-pages.
   * @param {string} pageName Page name.
   */
  OptionsPage.showPageByName = function(pageName) {
    // Notify main pages if they will be hidden.
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (name != pageName && page.willHidePage)
        page.willHidePage();
    }

    // Update the visibility attribute for main pages.
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      page.visible = name == pageName;
    }

    // Update the visibility attribute for sub-pages.
    for (var name in this.registeredSubPages) {
      var pageInfo = this.registeredSubPages[name];
      var match = name == pageName;
      if (match && document.documentElement.getAttribute('hide-menu') != 'true')
        pageInfo.parentPage.visible = true;
      pageInfo.page.visible = match;
    }

    // Notify main pages if they were shown.
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (name == pageName && page.didShowPage)
        page.didShowPage();
    }
  };

  /**
   * Called on load. Dispatch the URL hash to the given page's handleHash
   * function.
   * @param {string} pageName The string name of the (registered) options page.
   * @param {string} hash The value of the hash component of the URL.
   */
  OptionsPage.handleHashForPage = function(pageName, hash) {
    var page = this.registeredPages[pageName];
    if (!page) {
      page = this.registeredSubPages[pageName].page;
    }
    page.handleHash(hash);
  };

  /**
   * Shows a registered Overlay page.
   * @param {string} overlayName Page name.
   */
  OptionsPage.showOverlay = function(overlayName) {
    if (this.registeredOverlayPages[overlayName]) {
      this.registeredOverlayPages[overlayName].visible = true;
    }
  };

  /**
   * Clears overlays (i.e. hide all overlays).
   */
  OptionsPage.clearOverlays = function() {
     for (var name in this.registeredOverlayPages) {
       var page = this.registeredOverlayPages[name];
       page.visible = false;
     }
  };

  /**
   * Handle 'keydown' events.
   * @param {Event} e Key event.
   * @private
   */
  OptionsPage.onKeyDown_ = function(e) {
    if (e.keyCode == 27) { // Esc
      this.clearOverlays();
    }
  };

  /**
   * Closes any currently-open subpage.
   */
  OptionsPage.closeSubPage = function() {
    for (var name in this.registeredSubPages) {
      var pageInfo = this.registeredSubPages[name];
      if (pageInfo.page.visible) {
        pageInfo.page.visible = false;
        // Since the managed pref banner lives outside the overlay, and the
        // parent is not changing visibility, update the banner explicitly.
        pageInfo.parentPage.updateManagedBannerVisibility();
      }
    }
  };

  /**
  * Shows the tab contents for the given navigation tab.
  * @param {!Element} tab The tab that the user clicked.
  */
  OptionsPage.showTab = function(tab) {
    // Search parents until we find a tab, or the nav bar itself. This allows
    // tabs to have child nodes, e.g. labels in separately-styled spans.
    while (tab && !tab.classList.contains('subpages-nav-tabs') &&
           !tab.classList.contains('inactive-tab')) {
      tab = tab.parentNode;
    }
    if (!tab || !tab.classList.contains('inactive-tab'))
      return;

    if (this.activeNavTab != null) {
      this.activeNavTab.classList.remove('active-tab');
      $(this.activeNavTab.getAttribute('tab-contents')).classList.
          remove('active-tab-contents');
    }

    tab.classList.add('active-tab');
    $(tab.getAttribute('tab-contents')).classList.add('active-tab-contents');
    this.activeNavTab = tab;
  };

  /**
   * Registers new options page.
   * @param {OptionsPage} page Page to register.
   */
  OptionsPage.register = function(page) {
    this.registeredPages[page.name] = page;
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
  OptionsPage.registerSubPage = function(subPage, parentPage) {
    this.registeredSubPages[subPage.name] = {
        page: subPage, parentPage: parentPage };
    subPage.tab = undefined;
    subPage.isSubPageSheet = true;
    subPage.initializePage();
  };

  /**
   * Registers a new Overlay page.
   * @param {OptionsPage} page Page to register, must be a class derived from
   * OptionsPage.
   */
  OptionsPage.registerOverlay = function(page) {
    this.registeredOverlayPages[page.name] = page;
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
      this.showPageByName(data.pageName);
    }
  };

  /**
   * Initializes the complete options page.  This will cause
   * all C++ handlers to be invoked to do final setup.
   */
  OptionsPage.initialize = function() {
    chrome.send('coreOptionsInitialize');
    this.initialized_ = true;

    // Set up the overlay sheet. Clicks on the visible part of the parent page
    // should close the overlay, not fall through to the parent page.
    var self = this;
    var subpageContainer = $('subpage-sheet-container');
    subpageContainer.onclick = function(event) {
      if (!$('subpage-sheet').contains(event.target))
        self.closeSubPage();
      event.stopPropagation();
    }
    subpageContainer.onkeydown = function(e) {
      if (e.keyCode == 27)  // Esc
        self.closeSubPage();
    };
  };

  /**
   * Re-initializes the C++ handlers if necessary. This is called if the
   * handlers are torn down and recreated but the DOM may not have been (in
   * which case |initialize| won't be called again). If |initialize| hasn't been
   * called, this does nothing (since it will be later, once the DOM has
   * finished loading).
   */
  OptionsPage.reinitializeCore = function() {
    if (this.initialized_)
      chrome.send('coreOptionsInitialize');
  }

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
        this.updateManagedBannerVisibility();
      }
    },

    /**
     * Updates managed banner visibility state.
     */
    updateManagedBannerVisibility: function() {
      if (this.managed) {
        $('managed-prefs-banner').classList.remove('hidden');
      } else {
        $('managed-prefs-banner').classList.add('hidden');
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
        this.pageDiv.classList.remove('hidden');
        if (this.isOverlay) {
          $('overlay').classList.remove('hidden');
          document.addEventListener('keydown', OptionsPage.onKeyDown_);
        } else {
          if (this.isSubPageSheet)
            $('subpage-sheet-container').classList.remove('hidden');

          this.updateManagedBannerVisibility();

          // Recent webkit change no longer allows url change from "chrome://".
          window.history.pushState({pageName: this.name}, this.title);
        }
        if (this.tab) {
          this.tab.classList.add('navbar-item-selected');
        }
      } else {
        this.pageDiv.classList.add('hidden');
        if (this.isOverlay) {
          $('overlay').classList.add('hidden');
          document.removeEventListener('keydown', OptionsPage.onKeyDown_);
        } else if (this.isSubPageSheet) {
          $('subpage-sheet-container').classList.add('hidden');
        }
        if (this.tab) {
          this.tab.classList.remove('navbar-item-selected');
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
