// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
   * Gets the default page (to be shown on initial load).
   */
  OptionsPage.getDefaultPage = function() {
    return BrowserOptions.getInstance();
  };

  /**
   * Shows the default page.
   */
  OptionsPage.showDefaultPage = function() {
    this.navigateToPage(this.getDefaultPage().name);
  };

  /**
   * "Navigates" to a page, meaning that the page will be shown and the
   * appropriate entry is placed in the history.
   * @param {string} pageName Page name.
   */
  OptionsPage.navigateToPage = function(pageName) {
    this.showPageByName(pageName, true);
  };

  /**
   * Shows a registered page. This handles both top-level pages and sub-pages.
   * @param {string} pageName Page name.
   * @param {boolean} updateHistory True if we should update the history after
   *     showing the page.
   * @private
   */
  OptionsPage.showPageByName = function(pageName, updateHistory) {
    var targetPage = this.registeredPages[pageName];
    if (!targetPage || !targetPage.canShowPage()) {
      // If it's not a page, try it as an overlay.
      if (!targetPage && this.showOverlay_(pageName)) {
        if (updateHistory)
          this.updateHistoryState_();
        return;
      } else {
        targetPage = this.getDefaultPage();
      }
    }

    pageName = targetPage.name;

    // Determine if the root page is 'sticky', meaning that it
    // shouldn't change when showing a sub-page.  This can happen for special
    // pages like Search.
    var rootPage = null;
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (page.visible && !page.parentPage) {
        rootPage = page;
        break;
      }
    }
    var isRootPageLocked =
        rootPage && rootPage.sticky && targetPage.parentPage;

    // Notify pages if they will be hidden.
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (!page.parentPage && isRootPageLocked)
        continue;
      if (page.willHidePage && name != pageName &&
          !page.isAncestorOfPage(targetPage))
        page.willHidePage();
    }

    // Update visibilities to show only the hierarchy of the target page.
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (!page.parentPage && isRootPageLocked)
        continue;
      page.visible = name == pageName ||
          (!document.documentElement.classList.contains('hide-menu') &&
           page.isAncestorOfPage(targetPage));
    }

    // Update the history and current location.
    if (updateHistory)
      this.updateHistoryState_();

    // Always update the page title.
    document.title = targetPage.title;

    // Notify pages if they were shown.
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (!page.parentPage && isRootPageLocked)
        continue;
      if (page.didShowPage && (name == pageName ||
          page.isAncestorOfPage(targetPage)))
        page.didShowPage();
    }
  };

  /**
   * Updates the visibility and stacking order of the subpage backdrop
   * according to which subpage is topmost and visible.
   * @private
   */
  OptionsPage.updateSubpageBackdrop_ = function () {
    var topmostPage = this.getTopmostVisibleNonOverlayPage_();
    var nestingLevel = topmostPage ? topmostPage.nestingLevel : 0;

    var subpageBackdrop = $('subpage-backdrop');
    if (nestingLevel > 0) {
      var container = $('subpage-sheet-container-' + nestingLevel);
      subpageBackdrop.style.zIndex =
          parseInt(window.getComputedStyle(container).zIndex) - 1;
      subpageBackdrop.classList.remove('hidden');
    } else {
      subpageBackdrop.classList.add('hidden');
    }
  };

  /**
   * Pushes the current page onto the history stack, overriding the last page
   * if it is the generic chrome://settings/.
   * @private
   */
  OptionsPage.updateHistoryState_ = function() {
    var page = this.getTopmostVisiblePage();
    var path = location.pathname;
    if (path)
      path = path.slice(1);
    // The page is already in history (the user may have clicked the same link
    // twice). Do nothing.
    if (path == page.name)
      return;

    // If there is no path, the current location is chrome://settings/.
    // Override this with the new page.
    var historyFunction = path ? window.history.pushState :
                                 window.history.replaceState;
    historyFunction.call(window.history,
                         {pageName: page.name},
                         page.title,
                         '/' + page.name);
    // Update tab title.
    document.title = page.title;
  };

  /**
   * Shows a registered Overlay page. Does not update history.
   * @param {string} overlayName Page name.
   * @return {boolean} whether we showed an overlay.
   */
  OptionsPage.showOverlay_ = function(overlayName) {
    var overlay = this.registeredOverlayPages[overlayName];
    if (!overlay || !overlay.canShowPage())
      return false;

    if (overlay.parentPage)
      this.showPageByName(overlay.parentPage.name, false);

    this.registeredOverlayPages[overlayName].visible = true;
    return true;
  };

  /**
   * Returns whether or not an overlay is visible.
   * @return {boolean} True if an overlay is visible.
   * @private
   */
  OptionsPage.isOverlayVisible_ = function() {
    return this.getVisibleOverlay_() != null;
  };

  /**
   * Returns the currently visible overlay, or null if no page is visible.
   * @return {OptionPage} The visible overlay.
   */
  OptionsPage.getVisibleOverlay_ = function() {
    for (var name in this.registeredOverlayPages) {
      var page = this.registeredOverlayPages[name];
      if (page.visible)
        return page;
    }
    return null;
  };

  /**
   * Closes the visible overlay. Updates the history state after closing the
   * overlay.
   */
  OptionsPage.closeOverlay = function() {
    var overlay = this.getVisibleOverlay_();
    if (!overlay)
      return;

    overlay.visible = false;
    this.updateHistoryState_();
  };

  /**
   * Hides the visible overlay. Does not affect the history state.
   * @private
   */
  OptionsPage.hideOverlay_ = function() {
    var overlay = this.getVisibleOverlay_();
    if (overlay)
      overlay.visible = false;
  };

  /**
   * Returns the topmost visible page (overlays excluded).
   * @return {OptionPage} The topmost visible page aside any overlay.
   * @private
   */
  OptionsPage.getTopmostVisibleNonOverlayPage_ = function() {
    var topPage = null;
    for (var name in this.registeredPages) {
      var page = this.registeredPages[name];
      if (page.visible &&
          (!topPage || page.nestingLevel > topPage.nestingLevel))
        topPage = page;
    }

    return topPage;
  };

  /**
   * Returns the topmost visible page, or null if no page is visible.
   * @return {OptionPage} The topmost visible page.
   */
  OptionsPage.getTopmostVisiblePage = function() {
    // Check overlays first since they're top-most if visible.
    return this.getVisibleOverlay_() || this.getTopmostVisibleNonOverlayPage_();
  };

  /**
   * Closes the topmost open subpage, if any.
   * @private
   */
  OptionsPage.closeTopSubPage_ = function() {
    var topPage = this.getTopmostVisiblePage();
    if (topPage && !topPage.isOverlay && topPage.parentPage)
      topPage.visible = false;

    this.updateHistoryState_();
  };

  /**
   * Closes all subpages below the given level.
   * @param {number} level The nesting level to close below.
   */
  OptionsPage.closeSubPagesToLevel = function(level) {
    var topPage = this.getTopmostVisiblePage();
    while (topPage && topPage.nestingLevel > level) {
      topPage.visible = false;
      topPage = topPage.parentPage;
    }

    this.updateHistoryState_();
  };

  /**
   * Updates managed banner visibility state based on the topmost page.
   */
  OptionsPage.updateManagedBannerVisibility = function() {
    var topPage = this.getTopmostVisiblePage();
    if (topPage)
      topPage.updateManagedBannerVisibility();
  };

  /**
  * Shows the tab contents for the given navigation tab.
  * @param {!Element} tab The tab that the user clicked.
  */
  OptionsPage.showTab = function(tab) {
    // Search parents until we find a tab, or the nav bar itself. This allows
    // tabs to have child nodes, e.g. labels in separately-styled spans.
    while (tab && !tab.classList.contains('subpages-nav-tabs') &&
           !tab.classList.contains('tab')) {
      tab = tab.parentNode;
    }
    if (!tab || !tab.classList.contains('tab'))
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
    pageNav.textContent = page.pageDiv.querySelector('h1').textContent;
    pageNav.tabIndex = 0;
    pageNav.onclick = function(event) {
      OptionsPage.navigateToPage(this.getAttribute('pageName'));
    };
    pageNav.onkeypress = function(event) {
      // Enter or space
      if (event.keyCode == 13 || event.keyCode == 32) {
        OptionsPage.navigateToPage(this.getAttribute('pageName'));
      }
    };
    var navbar = $('navbar');
    navbar.appendChild(pageNav);
    page.tab = pageNav;
    page.initializePage();
  };

  /**
   * Find an enclosing section for an element if it exists.
   * @param {Element} element Element to search.
   * @return {OptionPage} The section element, or null.
   * @private
   */
  OptionsPage.findSectionForNode_ = function(node) {
    while (node = node.parentNode) {
      if (node.nodeName == 'SECTION')
        return node;
    }
    return null;
  };

  /**
   * Registers a new Sub-page.
   * @param {OptionsPage} subPage Sub-page to register.
   * @param {OptionsPage} parentPage Associated parent page for this page.
   * @param {Array} associatedControls Array of control elements that lead to
   *     this sub-page. The first item is typically a button in a root-level
   *     page. There may be additional buttons for nested sub-pages.
   */
  OptionsPage.registerSubPage = function(subPage,
                                         parentPage,
                                         associatedControls) {
    this.registeredPages[subPage.name] = subPage;
    subPage.parentPage = parentPage;
    if (associatedControls) {
      subPage.associatedControls = associatedControls;
      if (associatedControls.length) {
        subPage.associatedSection =
            this.findSectionForNode_(associatedControls[0]);
      }
    }
    subPage.tab = undefined;
    subPage.initializePage();
  };

  /**
   * Registers a new Overlay page.
   * @param {OptionsPage} overlay Overlay to register.
   * @param {OptionsPage} parentPage Associated parent page for this overlay.
   * @param {Array} associatedControls Array of control elements associated with
   *   this page.
   */
  OptionsPage.registerOverlay = function(overlay,
                                         parentPage,
                                         associatedControls) {
    this.registeredOverlayPages[overlay.name] = overlay;
    overlay.parentPage = parentPage;
    if (associatedControls) {
      overlay.associatedControls = associatedControls;
      if (associatedControls.length) {
        overlay.associatedSection =
            this.findSectionForNode_(associatedControls[0]);
      }
    }
    overlay.tab = undefined;
    overlay.isOverlay = true;
    overlay.initializePage();
  };

  /**
   * Callback for window.onpopstate.
   * @param {Object} data State data pushed into history.
   */
  OptionsPage.setState = function(data) {
    if (data && data.pageName) {
      // It's possible an overlay may be the last top-level page shown.
      if (this.isOverlayVisible_())
        this.hideOverlay_();

      this.showPageByName(data.pageName, false);
    }
  };

  /**
   * Freezes/unfreezes the scroll position of the top-level page based on
   * whether a subpage is showing.
   */
  OptionsPage.updateTopLevelPageFreezeState = function() {
    var freeze = OptionsPage.getTopmostVisiblePage().nestingLevel > 0;
    var topPageContainer = $('toplevel-page-container');
    if (topPageContainer.classList.contains('frozen') == freeze)
      return;

    if (freeze) {
      var scrollPosition = document.body.scrollTop;
      // Lock the width, since auto width computation will change.
      topPageContainer.style.width =
          window.getComputedStyle(topPageContainer).width;
      topPageContainer.classList.add('frozen');
      topPageContainer.style.top = -scrollPosition + 'px';
      this.updateFrozenPageHorizontalPosition_();
    } else {
      var scrollPosition = - parseInt(topPageContainer.style.top, 10);
      topPageContainer.classList.remove('frozen');
      topPageContainer.style.top = '';
      topPageContainer.style.left = '';
      topPageContainer.style.right = '';
      topPageContainer.style.width = '';
      // Restore the scroll position.
      window.scroll(document.body.scrollLeft, scrollPosition);
    }
  };

  /**
   * Initializes the complete options page.  This will cause all C++ handlers to
   * be invoked to do final setup.
   */
  OptionsPage.initialize = function() {
    chrome.send('coreOptionsInitialize');
    this.initialized_ = true;

    var self = this;
    // Close subpages if the user clicks on the html body. Listen in the
    // capturing phase so that we can stop the click from doing anything.
    document.body.addEventListener('click',
                                   this.bodyMouseEventHandler_.bind(this),
                                   true);
    // We also need to cancel mousedowns on non-subpage content.
    document.body.addEventListener('mousedown',
                                   this.bodyMouseEventHandler_.bind(this),
                                   true);

    // Hook up the close buttons.
    subpageCloseButtons = document.querySelectorAll('.close-subpage');
    for (var i = 0; i < subpageCloseButtons.length; i++) {
      subpageCloseButtons[i].onclick = function() {
        self.closeTopSubPage_();
      };
    };

    // Install handler for key presses.
    document.addEventListener('keydown', this.keyDownEventHandler_.bind(this));

    document.addEventListener('focus', this.manageFocusChange_.bind(this),
                              true);

    document.addEventListener('scroll', this.handleScroll_.bind(this));
    window.addEventListener('resize', this.handleResize_.bind(this));
    // Trigger the resize handler manually to set the initial state.
    this.handleResize_(null);
  };

  /**
   * Does a bounds check for the element on the given x, y client coordinates.
   * @param {Element} e The DOM element.
   * @param {number} x The client X to check.
   * @param {number} y The client Y to check.
   * @return {boolean} True if the point falls within the element's bounds.
   * @private
   */
  OptionsPage.elementContainsPoint_ = function(e, x, y) {
    var clientRect = e.getBoundingClientRect();
    return x >= clientRect.left && x <= clientRect.right &&
        y >= clientRect.top && y <= clientRect.bottom;
  };

  /**
   * Called when focus changes; ensures that focus doesn't move outside
   * the topmost subpage/overlay.
   * @param {Event} e The focus change event.
   * @private
   */
  OptionsPage.manageFocusChange_ = function(e) {
    var focusableItemsRoot;
    var topPage = this.getTopmostVisiblePage();
    if (!topPage)
      return;

    if (topPage.isOverlay) {
      // If an overlay is visible, that defines the tab loop.
      focusableItemsRoot = topPage.pageDiv;
    } else {
      // If a subpage is visible, use its parent as the tab loop constraint.
      // (The parent is used because it contains the close button.)
      if (topPage.nestingLevel > 0)
        focusableItemsRoot = topPage.pageDiv.parentNode;
    }

    if (focusableItemsRoot && !focusableItemsRoot.contains(e.target))
      topPage.focusFirstElement();
  };

  /**
   * Called when the page is scrolled; moves elements that are position:fixed
   * but should only behave as if they are fixed for vertical scrolling.
   * @param {Event} e The scroll event.
   * @private
   */
  OptionsPage.handleScroll_ = function(e) {
    var horizontalOffset = document.body.scrollLeft;
    // position:fixed doesn't seem to work for horizontal scrolling in RTL mode,
    // so only adjust in LTR mode (where scroll values will be positive).
    if (horizontalOffset >= 0) {
      $('navbar-container').style.left = -document.body.scrollLeft + 'px';
      this.updateFrozenPageHorizontalPosition_();
    }
  };

  /**
   * Updates any frozen pages to match the horizontal scroll position.
   * @param {Event} e The scroll event.
   * @private
   */
  OptionsPage.updateFrozenPageHorizontalPosition_ = function() {
    var horizontalOffset = document.body.scrollLeft;
    var toplevelPage = $('toplevel-page-container');
    if (toplevelPage.classList.contains('frozen')) {
      const WINDOW_LEFT_OFFSET = 291;  // Sidebar width + padding
      if (document.documentElement.dir == 'rtl') {
        toplevelPage.style.right = WINDOW_LEFT_OFFSET + 'px';
      } else {
        toplevelPage.style.left =
            WINDOW_LEFT_OFFSET -document.body.scrollLeft + 'px';
      }
    }
  };

  /**
   * Called when the page is resized; adjusts the size of elements that depend
   * on the veiwport.
   * @param {Event} e The resize event.
   * @private
   */
  OptionsPage.handleResize_ = function(e) {
    // Set an explicit height equal to the viewport on all the subpage
    // containers shorter than the viewport. This is used instead of
    // min-height: 100% so that there is an explicit height for the subpages'
    // min-height: 100%.
    var viewportHeight = document.documentElement.clientHeight;
    var subpageContainers =
        document.querySelectorAll('.subpage-sheet-container');
    for (var i = 0; i < subpageContainers.length; i++) {
      if (subpageContainers[i].scrollHeight > viewportHeight)
        subpageContainers[i].style.removeProperty('height');
      else
        subpageContainers[i].style.height = viewportHeight + 'px';
    }
  };

  /**
   * A function to handle mouse events (mousedown or click) on the html body by
   * closing subpages and/or stopping event propagation.
   * @return {Event} a mousedown or click event.
   * @private
   */
  OptionsPage.bodyMouseEventHandler_ = function(event) {
    // Do nothing if a subpage isn't showing.
    var topPage = this.getTopmostVisiblePage();
    if (!topPage || topPage.isOverlay || !topPage.parentPage)
      return;

    // Don't interfere with navbar clicks.
    if ($('navbar').contains(event.target))
      return;

    // Figure out which page the click happened in.
    for (var level = topPage.nestingLevel; level >= 0; level--) {
      var clickIsWithinLevel = level == 0 ? true :
          OptionsPage.elementContainsPoint_(
              $('subpage-sheet-' + level), event.clientX, event.clientY);

      if (!clickIsWithinLevel)
        continue;

      // Event was within the topmost page; do nothing.
      if (topPage.nestingLevel == level)
        return;

      // Block propgation of both clicks and mousedowns, but only close subpages
      // on click.
      if (event.type == 'click')
        this.closeSubPagesToLevel(level);
      event.stopPropagation();
      event.preventDefault();
      return;
    }
  };

  /**
   * A function to handle key press events.
   * @return {Event} a keydown event.
   * @private
   */
  OptionsPage.keyDownEventHandler_ = function(event) {
    // Close the top overlay or sub-page on esc.
    if (event.keyCode == 27) {  // Esc
      if (this.isOverlayVisible_())
        this.closeOverlay();
      else
        this.closeTopSubPage_();
    }
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
     * The parent page of this option page, or null for top-level pages.
     * @type {OptionsPage}
     */
    parentPage: null,

    /**
     * The section on the parent page that is associated with this page.
     * Can be null.
     * @type {Element}
     */
    associatedSection: null,

    /**
     * An array of controls that are associated with this page.  The first
     * control should be located on a top-level page.
     * @type {OptionsPage}
     */
    associatedControls: null,

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
     * Updates managed banner visibility state. This function iterates over
     * all input fields of a window and if any of these is marked as managed
     * it triggers the managed banner to be visible. The banner can be enforced
     * being on through the managed flag of this class but it can not be forced
     * being off if managed items exist.
     */
    updateManagedBannerVisibility: function() {
      var bannerDiv = $('managed-prefs-banner');

      var hasManaged = this.managed;
      if (!hasManaged) {
        var inputElements = this.pageDiv.querySelectorAll('input');
        for (var i = 0, len = inputElements.length; i < len; i++) {
          if (inputElements[i].managed) {
            hasManaged = true;
            break;
          }
        }
      }
      if (hasManaged) {
        bannerDiv.classList.remove('hidden');
        var height = window.getComputedStyle($('managed-prefs-banner')).height;
        $('subpage-backdrop').style.top = height;
      } else {
        bannerDiv.classList.add('hidden');
        $('subpage-backdrop').style.top = '0';
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
        var overlayWasVisible = OptionsPage.isOverlayVisible_();
        this.pageDiv.classList.remove('hidden');
        if (this.isOverlay) {
          $('overlay').classList.remove('hidden');
        } else {
          var nestingLevel = this.nestingLevel;
          if (nestingLevel > 0) {
            var containerId = 'subpage-sheet-container-' + nestingLevel;
            $(containerId).classList.remove('hidden');
          }
        }

        if (this.tab)
          this.tab.classList.add('navbar-item-selected');
      } else {
        this.pageDiv.classList.add('hidden');
        if (this.isOverlay) {
          $('overlay').classList.add('hidden');
        } else {
          var nestingLevel = this.nestingLevel;
          if (nestingLevel > 0) {
            var containerId = 'subpage-sheet-container-' + nestingLevel;
            $(containerId).classList.add('hidden');
          }
        }

        if (this.tab)
          this.tab.classList.remove('navbar-item-selected');
      }

      // A subpage was shown or hidden.
      if (!this.isOverlay && this.nestingLevel > 0) {
        OptionsPage.updateSubpageBackdrop_();
        OptionsPage.updateTopLevelPageFreezeState();
        if (visible) {
          // Scroll to the top of the newly-opened subpage.
          window.scroll(document.body.scrollLeft, 0)
        }
      }

      // The managed prefs banner is global, so after any visibility change
      // update it based on the topmost page, not necessarily this page
      // (e.g., if an ancestor is made visible after a child).
      OptionsPage.updateManagedBannerVisibility();

      cr.dispatchPropertyChange(this, 'visible', visible, !visible);
    },

    /**
     * Focuses the first control on the page.
     */
    focusFirstElement: function() {
      // Sets focus on the first interactive element in the page.
      var focusElement =
          this.pageDiv.querySelector('button, input, list, select');
      if (focusElement)
        focusElement.focus();
    },

    /**
     * The nesting level of this page.
     * @type {number} The nesting level of this page (0 for top-level page)
     */
    get nestingLevel() {
      var level = 0;
      var parent = this.parentPage;
      while (parent) {
        level++;
        parent = parent.parentPage;
      }
      return level;
    },

    /**
     * Whether the page is considered 'sticky', such that it will
     * remain a top-level page even if sub-pages change.
     * @type {boolean} True if this page is sticky.
     */
    get sticky() {
      return false;
    },

    /**
     * Checks whether this page is an ancestor of the given page in terms of
     * subpage nesting.
     * @param {OptionsPage} page
     * @return {boolean} True if this page is nested under |page|
     */
    isAncestorOfPage: function(page) {
      var parent = page.parentPage;
      while (parent) {
        if (parent == this)
          return true;
        parent = parent.parentPage;
      }
      return false;
    },

    /**
     * Whether it should be possible to show the page.
     * @return {boolean} True if the page should be shown
     */
    canShowPage: function() {
      return true;
    },
  };

  // Export
  return {
    OptionsPage: OptionsPage
  };
});
