// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{id: number, rawQuery: ?string, regExp: ?RegExp}} */
var SearchContext;

cr.define('settings', function() {
  /** @const {string} */
  var WRAPPER_CSS_CLASS = 'search-highlight-wrapper';

  /** @const {string} */
  var HIT_CSS_CLASS = 'search-highlight-hit';

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
   * TODO(dpapad): Consider making this a private method of TopLevelSearchTask.
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
   * Checks whether the given |node| requires force rendering.
   *
   * @param {!SearchContext} context
   * @param {!Node} node
   * @return {boolean} Whether a forced rendering task was scheduled.
   * @private
   */
  function forceRenderNeeded_(context, node) {
    if (node.nodeName != 'TEMPLATE' || !node.hasAttribute('name') || node.if)
      return false;

    // TODO(dpapad): Temporarily ignore site-settings because it throws an
    // assertion error during force-rendering.
    return node.getAttribute('name').indexOf('site-') != 0;
  }

  /**
   * Traverses the entire DOM (including Shadow DOM), finds text nodes that
   * match the given regular expression and applies the highlight UI. It also
   * ensures that <settings-section> instances become visible if any matches
   * occurred under their subtree.
   *
   * @param {!SearchContext} context
   * @param {!Node} root The root of the sub-tree to be searched
   * @private
   */
  function findAndHighlightMatches_(context, root) {
    function doSearch(node) {
      if (forceRenderNeeded_(context, node)) {
        SearchManager.getInstance().queue_.addRenderTask(
            new RenderTask(context, node));
        return;
      }

      if (IGNORED_ELEMENTS.has(node.nodeName))
        return;

      if (node.nodeType == Node.TEXT_NODE) {
        var textContent = node.nodeValue.trim();
        if (textContent.length == 0)
          return;

        if (context.regExp.test(textContent)) {
          revealParentSection_(node);
          highlight_(node, textContent.split(context.regExp));
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

    doSearch(root);
  }

  /**
   * Finds and makes visible the <settings-section> parent of |node|.
   * @param {!Node} node
   */
  function revealParentSection_(node) {
    // Find corresponding SETTINGS-SECTION parent and make it visible.
    var parent = node;
    while (parent && parent.nodeName !== 'SETTINGS-SECTION') {
      parent = parent.nodeType == Node.DOCUMENT_FRAGMENT_NODE ?
          parent.host : parent.parentNode;
    }
    if (parent)
      parent.hidden = false;
  }

  /**
   * @constructor
   *
   * @param {!SearchContext} context
   * @param {!Node} node
   */
  function Task(context, node) {
    /** @protected {!SearchContext} */
    this.context = context;

    /** @protected {!Node} */
    this.node = node;
  }

  Task.prototype = {
    /**
     * @abstract
     * @return {!Promise}
     */
    exec: function() {},
  };

  /**
   * A task that takes a <template is="dom-if">...</template> node corresponding
   * to a setting subpage and renders it. A SearchAndHighlightTask is posted for
   * the newly rendered subtree, once rendering is done.
   * @constructor
   * @extends {Task}
   *
   * @param {!SearchContext} context
   * @param {!Node} node
   */
  function RenderTask(context, node) {
    Task.call(this, context, node);
  }

  RenderTask.prototype = {
    /** @override */
    exec: function() {
      var subpageTemplate =
          this.node['_content'].querySelector('settings-subpage');
      subpageTemplate.id = subpageTemplate.id || this.node.getAttribute('name');
      assert(!this.node.if);
      this.node.if = true;

      return new Promise(function(resolve, reject) {
        var parent = this.node.parentNode;
        parent.async(function() {
          var renderedNode = parent.querySelector('#' + subpageTemplate.id);
          // Register a SearchAndHighlightTask for the part of the DOM that was
          // just rendered.
          SearchManager.getInstance().queue_.addSearchAndHighlightTask(
              new SearchAndHighlightTask(this.context, assert(renderedNode)));
          resolve();
        }.bind(this));
      }.bind(this));
    },
  };

  /**
   * @constructor
   * @extends {Task}
   *
   * @param {!SearchContext} context
   * @param {!Node} node
   */
  function SearchAndHighlightTask(context, node) {
    Task.call(this, context, node);
  }

  SearchAndHighlightTask.prototype = {
    /** @override */
    exec: function() {
      findAndHighlightMatches_(this.context, this.node);
      return Promise.resolve();
    },
  };

  /**
   * @constructor
   * @extends {Task}
   *
   * @param {!SearchContext} context
   * @param {!Node} page
   */
  function TopLevelSearchTask(context, page) {
    Task.call(this, context, page);
  }

  TopLevelSearchTask.prototype = {
    /** @override */
    exec: function() {
      findAndRemoveHighlights_(this.node);

      var shouldSearch = this.context.regExp !== null;
      this.setSectionsVisibility_(!shouldSearch);
      if (shouldSearch)
        findAndHighlightMatches_(this.context, this.node);

      return Promise.resolve();
    },

    /**
     * @param {boolean} visible
     * @private
     */
    setSectionsVisibility_: function(visible) {
      var sections = Polymer.dom(
          this.node.root).querySelectorAll('settings-section');
      for (var i = 0; i < sections.length; i++)
        sections[i].hidden = !visible;
    },
  };

  /**
   * @constructor
   */
  function TaskQueue() {
    /**
     * @private {{
     *   high: !Array<!Task>,
     *   middle: !Array<!Task>,
     *   low: !Array<!Task>
     * }}
     */
    this.queues_;
    this.reset();

    /**
     * Whether a task is currently running.
     * @private {boolean}
     */
    this.running_ = false;
  }

  TaskQueue.prototype = {
    /** Drops all tasks. */
    reset: function() {
      this.queues_ = {high: [], middle: [], low: []};
    },

    /** @param {!TopLevelSearchTask} task */
    addTopLevelSearchTask: function(task) {
      this.queues_.high.push(task);
      this.consumePending_();
    },

    /** @param {!SearchAndHighlightTask} task */
    addSearchAndHighlightTask: function(task) {
      this.queues_.middle.push(task);
      this.consumePending_();
    },

    /** @param {!RenderTask} task */
    addRenderTask: function(task) {
      this.queues_.low.push(task);
      this.consumePending_();
    },

    /**
     * @return {!Task|undefined}
     * @private
     */
    popNextTask_: function() {
      return this.queues_.high.shift() ||
          this.queues_.middle.shift() ||
          this.queues_.low.shift();
    },

    /** @private */
    consumePending_: function() {
      if (this.running_)
        return;

      while (1) {
        var task = this.popNextTask_();
        if (!task) {
          this.running_ = false;
          return;
        }

        window.requestIdleCallback(function() {
          function startNextTask() {
            this.running_ = false;
            this.consumePending_();
          }
          if (task.context.id ==
              SearchManager.getInstance().activeContext_.id) {
            task.exec().then(startNextTask.bind(this));
          } else {
            // Dropping this task without ever executing it, since a new search
            // has been issued since this task was queued.
            startNextTask.call(this);
          }
        }.bind(this));
        return;
      }
    },
  };

  /**
   * @constructor
   */
  var SearchManager = function() {
    /** @private {!TaskQueue} */
    this.queue_ = new TaskQueue();

    /** @private {!SearchContext} */
    this.activeContext_ = {id: 0, rawQuery: null, regExp: null};
  };
  cr.addSingletonGetter(SearchManager);

  /** @private @const {!RegExp} */
  SearchManager.SANITIZE_REGEX_ = /[-[\]{}()*+?.,\\^$|#\s]/g;

  SearchManager.prototype = {
    /**
     * @param {string} text The text to search for.
     * @param {!Node} page
     */
    search: function(text, page) {
      if (this.activeContext_.rawQuery != text) {
        var newId = this.activeContext_.id + 1;

        var regExp = null;
        // Generate search text by escaping any characters that would be
        // problematic for regular expressions.
        var searchText = text.trim().replace(
            SearchManager.SANITIZE_REGEX_, '\\$&');
        if (searchText.length > 0)
          regExp = new RegExp('(' + searchText + ')', 'i');

        this.activeContext_ = {id: newId, rawQuery: text, regExp: regExp};

        // Drop all previously scheduled tasks, since a new search was just
        // issued.
        this.queue_.reset();
      }

      this.queue_.addTopLevelSearchTask(
          new TopLevelSearchTask(this.activeContext_, page));
    },
  };

  /**
   * @param {string} text
   * @param {!Node} page
   */
  function search(text, page) {
    SearchManager.getInstance().search(text, page);
  }

  return {
    search: search,
  };
});
