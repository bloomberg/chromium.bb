// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * Constructor for FocusManager singleton. Checks focus of elements to ensure
   * that elements in "background" pages (i.e., those in a dialog that is not
   * the topmost overlay) do not receive focus.
   * @constructor
   */
  function FocusManager() {
  }

  cr.addSingletonGetter(FocusManager);

  FocusManager.prototype = {
    /**
     * Whether focus is being transferred backward or forward through the DOM.
     * @type {boolean}
     * @private
     */
    focusDirBackwards_: false,

    /**
     * Determines whether the |child| is a descendant of |parent| in the page's
     * DOM.
     * @param {Element} parent The parent element to test.
     * @param {Element} child The child element to test.
     * @return {boolean} True if |child| is a descendant of |parent|.
     * @private
     */
    isDescendantOf_: function(parent, child) {
      var current = child;

      while (current) {
        current = current.parentNode;
        if (typeof(current) == 'undefined' ||
            typeof(current) == 'null' ||
            current === document.body) {
          return false;
        } else if (current === parent) {
          return true;
        }
      }

      return false;
    },

    /**
     * Returns the container of the topmost visible page.
     * @return {Element} The topmost page.
     * @private
     */
    getTopmostPage_: function() {
      var topmostPage = OptionsPage.getTopmostVisiblePage().pageDiv;

      // The default page includes a search field that is a sibling of the
      // rest of the page instead of a child. Thus, use the parent node to
      // allow the search field to receive focus.
      if (topmostPage === OptionsPage.getDefaultPage().pageDiv)
        return topmostPage.parentNode;

      return topmostPage;
    },

    /**
     * Returns the elements on the page capable of receiving focus.
     * @return {Array.Element} The focusable elements.
     */
    getFocusableElements_: function() {
      var topmostPage = this.getTopmostPage_();

      // Create a TreeWalker object to traverse the DOM from |topmostPage|.
      var treeWalker = document.createTreeWalker(
          topmostPage,
          NodeFilter.SHOW_ELEMENT,
          { acceptNode: function(node) {
              // Reject all hidden nodes. FILTER_REJECT also rejects these
              // nodes' children, so non-hidden elements that are descendants of
              // hidden <div>s will correctly be rejected.
              if (node.hidden)
                return NodeFilter.FILTER_REJECT;

              // Skip nodes that cannot receive focus. FILTER_SKIP does not
              // cause this node's children also to be skipped.
              if (node.tabIndex < 0)
                return NodeFilter.FILTER_SKIP;

              // Accept nodes that are non-hidden and focusable.
              return NodeFilter.FILTER_ACCEPT;
            }
          },
          false);

      var focusable = [];
      while (treeWalker.nextNode())
        focusable.push(treeWalker.currentNode);

      return focusable;
    },

    /**
     * Retrieves the page's focusable elements and, if there are any, focuses
     * the first one.
     * @private
     */
    focusFirstElement_: function() {
      var focusableElements = this.getFocusableElements_();
      if (focusableElements.length != 0)
        focusableElements[0].focus();
    },

    /**
     * Retrieves the page's focusable elements and, if there are any, focuses
     * the last one.
     * @private
     */
    focusLastElement_: function() {
      var focusableElements = this.getFocusableElements_();
      if (focusableElements.length != 0)
        focusableElements[focusableElements.length - 1].focus();
    },

    /**
     * Attempts to focus the appropriate element in the current dialog.
     * @private
     */
    setFocus_: function() {
      // If |this.focusDirBackwards_| is true, the user has pressed "Shift+Tab"
      // and has caused the focus to be transferred backward, outside of the
      // current dialog. In this case, loop around and try to focus the last
      // element of the dialog; otherwise, try to focus the first element of the
      // dialog.
      if (this.focusDirBackwards_)
        this.focusLastElement_();
      else
        this.focusFirstElement_();
    },

    /**
     * Attempts to focus the first element in the current dialog.
     */
    focusFirstElement: function() {
      this.focusFirstElement_();
    },

    /**
     * Handler for focus events on the page.
     * @param {Event} event The focus event.
     * @private
     */
    onDocumentFocus_: function(event) {
      // If the element being focused is a descendant of the currently visible
      // page, focus is valid.
      if (this.isDescendantOf_(this.getTopmostPage_(), event.target))
        return;

      // The target of the focus event is not in the topmost visible page and
      // should not be focused.
      event.target.blur();

      this.setFocus_();
    },

    /**
     * Handler for keydown events on the page.
     * @param {Event} event The keydown event.
     * @private
     */
    onDocumentKeyDown_: function(event) {
      /** @const */ var tabKeyCode = 9;

      if (event.keyCode == tabKeyCode) {
        // If the "Shift" key is held, focus is being transferred backward in
        // the page.
        this.focusDirBackwards_ = event.shiftKey ? true : false;
      }
    },

    /**
     * Initializes the FocusManager by listening for events in the document.
     */
    initialize: function() {
      document.addEventListener('focus', this.onDocumentFocus_.bind(this),
          true);
      document.addEventListener('keydown', this.onDocumentKeyDown_.bind(this),
          true);
    },
  };

  return {
    FocusManager: FocusManager,
  };
});
