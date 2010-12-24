// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of the search page.
   * @constructor
   */
  function SearchPage() {
    OptionsPage.call(this, 'search', templateData.searchPage, 'searchPage');
    this.searchActive = false;
  }

  cr.addSingletonGetter(SearchPage);

  SearchPage.prototype = {
    // Inherit SearchPage from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;

      // Create a search field element.
      var searchField = document.createElement('input');
      searchField.id = 'search-field';
      searchField.type = 'search';
      searchField.setAttribute('autosave', 'org.chromium.options.search');
      searchField.setAttribute('results', '10');
      searchField.setAttribute('incremental', 'true');

      // Replace the contents of the navigation tab with the search field.
      self.tab.textContent = '';
      self.tab.appendChild(searchField);

      // Handle search events. (No need to throttle, WebKit's search field
      // will do that automatically.)
      searchField.onsearch = function(e) {
        self.setSearchText_(this.value);
      };
    },

    /**
     * @inheritDoc
     */
    get sticky() {
      return true;
    },

    /**
     * Called after this page has shown.
     */
    didShowPage: function() {
      // This method is called by the Options page after all pages have
      // had their visibilty attribute set.  At this point we can perform the
      // search specific DOM manipulation.
      this.setSearchActive_(true);
    },

    /**
     * Called before this page will be hidden.
     */
    willHidePage: function() {
      // This method is called by the Options page before all pages have
      // their visibilty attribute set.  Before that happens, we need to
      // undo the search specific DOM manipulation that was performed in
      // didShowPage.
      this.setSearchActive_(false);
    },

    /**
     * Update the UI to reflect whether we are in a search state.
     * @param {boolean} active True if we are on the search page.
     * @private
     */
    setSearchActive_: function(active) {
      // It's fine to exit if search wasn't active and we're not going to
      // activate it now.
      if (!this.searchActive_ && !active)
        return;

      if (this.searchActive_ != active) {
        this.searchActive_ = active;
        if (active) {
          // Reset the search criteria, effectively hiding all the sections.
          this.setSearchText_('');
        } else {
          // Just wipe out any active search text since it's no longer relevant.
          $('search-field').value = '';
        }
      }

      var pagesToSearch = this.getSearchablePages_();
      for (var key in pagesToSearch) {
        var page = pagesToSearch[key];

        if (!active)
          page.visible = false;

        // Update the visible state of all top-level elements that are not
        // sections (ie titles, button strips).  We do this before changing
        // the page visibility to avoid excessive re-draw.
        var length = page.pageDiv.childNodes.length;
        var childDiv;
        for (var i = 0; i < length; i++) {
          childDiv = page.pageDiv.childNodes[i];
          if (childDiv.nodeType == document.ELEMENT_NODE) {
            if (active) {
              if (childDiv.nodeName.toLowerCase() != 'section')
                childDiv.classList.add('search-hidden');
            } else {
              childDiv.classList.remove('search-hidden');
            }
          }
        }

        if (active) {
          // When search is active, remove the 'hidden' tag.  This tag may have
          // been added by the OptionsPage.
          page.pageDiv.classList.remove('hidden');
        }
      }

      // After hiding all page content, remove any highlighted matches.
      if (!active)
        this.unhighlightMatches_();
    },

    /**
     * Set the current search criteria.
     * @param {string} text Search text.
     * @private
     */
    setSearchText_: function(text) {
      var foundMatches = false;

      // Remove any highlighted matches.
      this.unhighlightMatches_();

      // Generate search text by applying lowercase and escaping any characters
      // that would be problematic for regular expressions.
      var searchText =
          text.toLowerCase().replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');

      // Generate a regular expression and replace string for hilighting
      // search terms.
      var regEx = new RegExp('(\\b' + searchText + ')', 'ig');
      var replaceString = '<span class="search-highlighted">$1</span>';

      // Initialize all sections.  If the search string matches a title page,
      // show sections for that page.
      var page, pageMatch, childDiv;
      var pagesToSearch = this.getSearchablePages_();
      for (var key in pagesToSearch) {
        page = pagesToSearch[key];
        pageMatch = false;
        if (searchText.length) {
          pageMatch = this.performReplace_(regEx, replaceString, page.tab);
        }
        if (pageMatch)
          foundMatches = true;
        for (var i = 0; i < page.pageDiv.childNodes.length; i++) {
          childDiv = page.pageDiv.childNodes[i];
          if (childDiv.nodeType == document.ELEMENT_NODE &&
              childDiv.nodeName == 'SECTION') {
            if (pageMatch) {
              childDiv.classList.remove('search-hidden');
            } else {
              childDiv.classList.add('search-hidden');
            }
          }
        }
      }

      if (searchText.length) {
        // Search all sub-pages, generating an array of top-level sections that
        // we need to make visible.
        var subPagesToSearch = this.getSearchableSubPages_();
        var control, node;
        for (var key in subPagesToSearch) {
          page = subPagesToSearch[key];
          if (this.performReplace_(regEx, replaceString, page.pageDiv)) {
            section = page.associatedSection;
            if (section)
              section.classList.remove('search-hidden');
            controls = page.associatedControls;
            if (controls) {
              // TODO(csilv): highlight each control.
            }

            foundMatches = true;
          }
        }

        // Search all top-level sections for anchored string matches.
        for (var key in pagesToSearch) {
          page = pagesToSearch[key];
          for (var i = 0; i < page.pageDiv.childNodes.length; i++) {
            childDiv = page.pageDiv.childNodes[i];
            if (childDiv.nodeType == document.ELEMENT_NODE &&
                childDiv.nodeName == 'SECTION' &&
                this.performReplace_(regEx, replaceString, childDiv)) {
              childDiv.classList.remove('search-hidden');
              foundMatches = true;
            }
          }
        }
      }

      // Configure elements on the search results page based on search results.
      if (searchText.length == 0) {
        $('searchPageInfo').classList.remove('search-hidden');
        $('searchPageNoMatches').classList.add('search-hidden');
      } else if (foundMatches) {
        $('searchPageInfo').classList.add('search-hidden');
        $('searchPageNoMatches').classList.add('search-hidden');
      } else {
        $('searchPageInfo').classList.add('search-hidden');
        $('searchPageNoMatches').classList.remove('search-hidden');
      }
    },

    /**
     * Performs a string replacement based on a regex and replace string.
     * @param {RegEx} regex A regular expression for finding search matches.
     * @param {String} replace A string to apply the replace operation.
     * @param {Element} element An HTML container element.
     * @returns {Boolean} true if the element was changed.
     * @private
     */
    performReplace_: function(regex, replace, element) {
      var found = false;
      var div, child, tmp;

      // Walk the tree, searching each TEXT node.
      var walker = document.createTreeWalker(element,
                                             NodeFilter.SHOW_TEXT,
                                             null,
                                             false);
      var node = walker.nextNode();
      while (node) {
        // Perform a search and replace on the text node value.
        var newValue = node.nodeValue.replace(regex, replace);
        if (newValue != node.nodeValue) {
          // The text node has changed so that means we found at least one
          // match.
          found = true;

          // Create a temporary div element and set the innerHTML to the new
          // value.
          div = document.createElement('div');
          div.innerHTML = newValue;

          // Insert all the child nodes of the temporary div element into the
          // document, before the original node.
          child = div.firstChild;
          while (child = div.firstChild) {
            node.parentNode.insertBefore(child, node);
          };

          // Delete the old text node and advance the walker to the next
          // node.
          tmp = node;
          node = walker.nextNode();
          tmp.parentNode.removeChild(tmp);
        } else {
          node = walker.nextNode();
        }
      }

      return found;
    },

    /**
     * Removes all search highlight tags from the document.
     * @private
     */
    unhighlightMatches_: function() {
      // Find all search highlight elements.
      var elements = document.querySelectorAll('.search-highlighted');

      // For each element, remove the highlighting.
      var node, parent, i, length = elements.length;
      for (i = 0; i < length; i++) {
        node = elements[i];
        parent = node.parentNode;

        // Replace the highlight element with the first child (the text node).
        parent.replaceChild(node.firstChild, node);

        // Normalize the parent so that multiple text nodes will be combined.
        parent.normalize();
      }
    },

    /**
     * Builds a list of top-level pages to search.  Omits the search page and
     * all sub-pages.
     * @returns {Array} An array of pages to search.
     * @private
     */
    getSearchablePages_: function() {
      var name, page, pages = [];
      for (name in OptionsPage.registeredPages) {
        if (name != this.name) {
          page = OptionsPage.registeredPages[name];
          if (!page.parentPage)
            pages.push(page);
        }
      }
      return pages;
    },

    /**
     * Builds a list of sub-pages (and overlay pages) to search.  Ignore pages
     * that have no associated controls.
     * @returns {Array} An array of pages to search.
     * @private
     */
    getSearchableSubPages_: function() {
      var name, pageInfo, page, pages = [];
      for (name in OptionsPage.registeredPages) {
        page = OptionsPage.registeredPages[name];
        if (page.parentPage && page.associatedSection)
          pages.push(page);
      }
      for (name in OptionsPage.registeredOverlayPages) {
        page = OptionsPage.registeredOverlayPages[name];
        if (page.associatedSection && page.pageDiv != undefined)
          pages.push(page);
      }
      return pages;
    }
  };

  // Export
  return {
    SearchPage: SearchPage
  };

});
