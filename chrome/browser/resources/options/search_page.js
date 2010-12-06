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
      searchField.id = 'searchField';
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
          $('searchField').value = '';
        }
      }

      var page, length, childDiv;
      for (var name in OptionsPage.registeredPages) {
        if (name == this.name)
          continue;

        // Update the visible state of all top-level elements that are not
        // sections (ie titles, button strips).  We do this before changing
        // the page visibility to avoid excessive re-draw.
        page = OptionsPage.registeredPages[name];
        length = page.pageDiv.childNodes.length;
        for (var i = 0; i < length; i++) {
          childDiv = page.pageDiv.childNodes[i];
          if (childDiv.nodeType == 1) {
            if (active) {
              if (childDiv.nodeName.toLowerCase() != 'section')
                childDiv.classList.add('search-hidden');
            } else {
                childDiv.classList.remove('search-hidden');
            }
          }
        }

        // Toggle the visibility state of the page.
        if (active) {
          // When search is active, remove the 'hidden' tag.  This tag may have
          // been added by the OptionsPage.
          page.pageDiv.classList.remove('hidden');
        } else {
          page.visible = false;
        }
      }
    },

    /**
     * Set the current search criteria.
     * @param {string} text Search text.
     * @private
     */
    setSearchText_: function(text) {
      var searchText = text.toLowerCase();
      var foundMatches = false;

      // Build a list of pages to search.  Omit the search page.
      var pagesToSearch = [];
      for (var name in OptionsPage.registeredPages) {
        if (name != this.name)
          pagesToSearch.push(OptionsPage.registeredPages[name]);
      }

      // Hide all sections.  If the search string matches a title page, show
      // all sections of that page.
      for (var key in pagesToSearch) {
        var page = pagesToSearch[key];
        var pageTitle = page.title.toLowerCase();
        // Hide non-sections in each page.
        for (var i = 0; i < page.pageDiv.childNodes.length; i++) {
          var childDiv = page.pageDiv.childNodes[i];
          if (childDiv.nodeType == 1 &&
              childDiv.nodeName.toLowerCase() == 'section') {
            if (pageTitle == searchText) {
              childDiv.classList.remove('search-hidden');
              foundMatches = true;
            } else {
              childDiv.classList.add('search-hidden');
            }
          }
        }
      }

      // Now search all sections for anchored string matches.
      if (!foundMatches && searchText.length) {
        var searchRegEx = new RegExp('\\b' + searchText, 'i');
        for (var key in pagesToSearch) {
          var page = pagesToSearch[key];
          for (var i = 0; i < page.pageDiv.childNodes.length; i++) {
            var childDiv = page.pageDiv.childNodes[i];
            if (childDiv.nodeType == 1 &&
                childDiv.nodeName.toLowerCase() == 'section') {
              var isMatch = false;
              var sectionElements = childDiv.getElementsByTagName("*");
              var length = sectionElements.length;
              var element;
              for (var j = 0; j < length; j++) {
                element = sectionElements[j];
                if (searchRegEx.test(element.textContent)) {
                  isMatch = true;
                  break;
                }
              }
              if (isMatch) {
                childDiv.classList.remove('search-hidden');
                foundMatches = true;
              }
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
    }
  };

  // Export
  return {
    SearchPage: SearchPage
  };

});
