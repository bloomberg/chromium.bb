// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @const {string} */
  var WRAPPER_CSS_CLASS = 'search-highlight-wrapper';

  /** @const {string} */
  var HIT_CSS_CLASS = 'search-highlight-hit';

  /** @const {!RegExp} */
  var SANITIZE_REGEX = /[-[\]{}()*+?.,\\^$|#\s]/g;

  /**
   * List of elements types that should not be searched at all.
   * The only DOM-MODULE node is in <body> which is not searched, therefore
   * DOM-MODULE is not needed in this set.
   * @const {!Set<string>}
   */
  var IGNORED_ELEMENTS = new Set([
    'CONTENT',
    'CR-EVENTS',
    'IMG',
    'IRON-ICON',
    'IRON-LIST',
    'PAPER-ICON-BUTTON',
    /* TODO(dpapad): paper-item is used for dynamically populated dropdown
     * menus. Perhaps a better approach is to mark the entire dropdown menu such
     * that search algorithm can skip it as a whole instead.
     */
    'PAPER-ITEM',
    'PAPER-RIPPLE',
    'PAPER-SLIDER',
    'PAPER-SPINNER',
    'STYLE',
    'TEMPLATE',
  ]);

  /**
   * Finds all previous highlighted nodes under |node| (both within self and
   * children's Shadow DOM) and removes the highlight (yellow rectangle).
   * @param {!Node} node
   * @private
   */
  function findAndRemoveHighlights_(node) {
    var wrappers = node.querySelectorAll('* /deep/ .' + WRAPPER_CSS_CLASS);

    for (var wrapper of wrappers) {
      var hitElements = wrapper.querySelectorAll('.' + HIT_CSS_CLASS);
      // For each hit element, remove the highlighting.
      for (var hitElement of hitElements) {
        wrapper.replaceChild(hitElement.firstChild, hitElement);
      }

      // Normalize so that adjacent text nodes will be combined.
      wrapper.normalize();
      // Restore the DOM structure as it was before the search occurred.
      if (wrapper.previousSibling)
        wrapper.textContent = ' ' + wrapper.textContent;
      if (wrapper.nextSibling)
        wrapper.textContent = wrapper.textContent + ' ';

      wrapper.parentElement.replaceChild(wrapper.firstChild, wrapper);
    }
  }

  /**
   * Applies the highlight UI (yellow rectangle) around all matches in |node|.
   * @param {!Node} node The text node to be highlighted. |node| ends up
   *     being removed from the DOM tree.
   * @param {!Array<string>} tokens The string tokens after splitting on the
   *     relevant regExp. Even indices hold text that doesn't need highlighting,
   *     odd indices hold the text to be highlighted. For example:
   *     var r = new RegExp('(foo)', 'i');
   *     'barfoobar foo bar'.split(r) => ['bar', 'foo', 'bar ', 'foo', ' bar']
   * @private
   */
  function highlight_(node, tokens) {
    var wrapper = document.createElement('span');
    wrapper.classList.add(WRAPPER_CSS_CLASS);
    // Use existing node as placeholder to determine where to insert the
    // replacement content.
    node.parentNode.replaceChild(wrapper, node);

    for (var i = 0; i < tokens.length; ++i) {
      if (i % 2 == 0) {
        wrapper.appendChild(document.createTextNode(tokens[i]));
      } else {
        var span = document.createElement('span');
        span.classList.add(HIT_CSS_CLASS);
        span.style.backgroundColor = 'yellow';
        span.textContent = tokens[i];
        wrapper.appendChild(span);
      }
    }
  }

  /**
   * Traverses the entire DOM (including Shadow DOM), finds text nodes that
   * match the given regular expression and applies the highlight UI. It also
   * ensures that <settings-section> instances become visible if any matches
   * occurred under their subtree.
   *
   * @param {!Element} page The page to be searched, should be either
   *     <settings-basic-page> or <settings-advanced-page>.
   * @param {!RegExp} regExp The regular expression to detect matches.
   * @private
   */
  function findAndHighlightMatches_(page, regExp) {
    function doSearch(node) {
      if (IGNORED_ELEMENTS.has(node.tagName))
        return;

      if (node.nodeType == Node.TEXT_NODE) {
        var textContent = node.nodeValue.trim();
        if (textContent.length == 0)
          return;

        if (regExp.test(textContent)) {
          revealParentSection_(node);
          highlight_(node, textContent.split(regExp));
        }
        // Returning early since TEXT_NODE nodes never have children.
        return;
      }

      var child = node.firstChild;
      while (child !== null) {
        // Getting a reference to the |nextSibling| before calling doSearch()
        // because |child| could be removed from the DOM within doSearch().
        var nextSibling = child.nextSibling;
        doSearch(child);
        child = nextSibling;
      }

      var shadowRoot = node.shadowRoot;
      if (shadowRoot)
        doSearch(shadowRoot);
    }

    doSearch(page);
  }

  /**
   * Finds and makes visible the <settings-section> parent of |node|.
   * @param {!Node} node
   */
  function revealParentSection_(node) {
    // Find corresponding SETTINGS-SECTION parent and make it visible.
    var parent = node;
    while (parent && parent.tagName !== 'SETTINGS-SECTION') {
      parent = parent.nodeType == Node.DOCUMENT_FRAGMENT_NODE ?
          parent.host : parent.parentNode;
    }
    if (parent)
      parent.hidden = false;
  }

  /**
   * @param {!Element} page
   * @param {boolean} visible
   * @private
   */
  function setSectionsVisibility_(page, visible) {
    var sections = Polymer.dom(page.root).querySelectorAll('settings-section');
    for (var i = 0; i < sections.length; i++)
      sections[i].hidden = !visible;
  }

  /**
   * Performs hierarchical search, starting at the given page element.
   * @param {string} text
   * @param {!Element} page Must be either <settings-basic-page> or
   *     <settings-advanced-page>.
   */
  function search(text, page) {
    findAndRemoveHighlights_(page);

    // Generate search text by escaping any characters that would be problematic
    // for regular expressions.
    var searchText = text.trim().replace(SANITIZE_REGEX, '\\$&');
    if (searchText.length == 0) {
      setSectionsVisibility_(page, true);
      return;
    }

    setSectionsVisibility_(page, false);
    findAndHighlightMatches_(page, new RegExp('(' + searchText + ')', 'i'));
  }

  return {
    search: search,
  };
});
