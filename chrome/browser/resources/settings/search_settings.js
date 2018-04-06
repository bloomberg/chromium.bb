// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * A data structure used by callers to combine the results of multiple search
 * requests.
 *
 * @typedef {{
 *   canceled: Boolean,
 *   didFindMatches: Boolean,
 *   rawQuery: string,
 *   wasClearSearch: Boolean,
 * }}
 */
settings.SearchResult;

cr.define('settings', function() {
  /**
   * A CSS attribute indicating that a node should be ignored during searching.
   * @type {string}
   */
  const SKIP_SEARCH_CSS_ATTRIBUTE = 'no-search';

  /**
   * List of elements types that should not be searched at all.
   * The only DOM-MODULE node is in <body> which is not searched, therefore
   * DOM-MODULE is not needed in this set.
   * @type {!Set<string>}
   */
  const IGNORED_ELEMENTS = new Set([
    'CONTENT',
    'CR-EVENTS',
    'DIALOG',
    'IMG',
    'IRON-ICON',
    'IRON-LIST',
    'PAPER-ICON-BUTTON',
    'PAPER-RIPPLE',
    'PAPER-SLIDER',
    'PAPER-SPINNER-LITE',
    'STYLE',
    'TEMPLATE',
  ]);

  /**
   * Finds all previous highlighted nodes under |node| (both within self and
   * children's Shadow DOM) and replaces the highlights (yellow rectangle and
   * search bubbles) with the original text node.
   * TODO(dpapad): Consider making this a private method of TopLevelSearchTask.
   * @param {!Array<!Node>} nodes
   * @private
   */
  function findAndRemoveHighlights_(nodes) {
    nodes.forEach(node => {
      cr.search_highlight_utils.findAndRemoveHighlights(node);
      cr.search_highlight_utils.findAndRemoveBubbles(node);
    });
  }

  /**
   * Traverses the entire DOM (including Shadow DOM), finds text nodes that
   * match the given regular expression and applies the highlight UI. It also
   * ensures that <settings-section> instances become visible if any matches
   * occurred under their subtree.
   *
   * @param {!settings.SearchRequest} request
   * @param {!Array<!Node>} roots The roots of the sub-trees to be searched.
   * @param {!function(!Node): void} addTextObserver
   * @private
   */
  function findAndHighlightMatches_(request, roots, addTextObserver) {
    let foundMatches = false;
    function doSearch(node) {
      if (node.nodeName == 'TEMPLATE' && node.hasAttribute('route-path') &&
          !node.if && !node.hasAttribute(SKIP_SEARCH_CSS_ATTRIBUTE)) {
        request.queue_.addRenderTask(
            new RenderTask(request, node, addTextObserver));
        return false;
      }

      if (IGNORED_ELEMENTS.has(node.nodeName))
        return false;

      if (node instanceof HTMLElement) {
        const element = /** @type {HTMLElement} */ (node);
        if (element.hasAttribute(SKIP_SEARCH_CSS_ATTRIBUTE) ||
            element.hasAttribute('hidden') || element.style.display == 'none') {
          return false;
        }
      }

      if (node.nodeType == Node.TEXT_NODE) {
        addTextObserver(node);
        const textContent = node.nodeValue.trim();
        if (textContent.length == 0)
          return false;

        if (request.regExp.test(textContent)) {
          foundMatches = true;
          revealParentSection_(node, request.rawQuery_);

          // Don't highlight <select> nodes, yellow rectangles can't be
          // displayed within an <option>.
          // TODO(dpapad): highlight <select> controls with a search bubble
          // instead.
          if (node.parentNode.nodeName != 'OPTION') {
            cr.search_highlight_utils.highlight(
                node, textContent.split(request.regExp));
          }
        }
        // Returning early since TEXT_NODE nodes never have children.
        return false;
      }

      return true;
    }

    const nodes = Array.from(roots);
    while (nodes.length > 0) {
      const node = nodes.pop();
      const continueSearchChildren = doSearch(node);
      if (continueSearchChildren) {
        nodes.push(...node.childNodes);
        if (node.shadowRoot)
          nodes.push(node.shadowRoot);
      }
    }

    return foundMatches;
  }

  /**
   * Finds and makes visible the <settings-section> parent of |node|.
   * @param {!Node} node
   * @param {string} rawQuery
   * @private
   */
  function revealParentSection_(node, rawQuery) {
    let associatedControl = null;
    // Find corresponding SETTINGS-SECTION parent and make it visible.
    let parent = node;
    while (parent && parent.nodeName !== 'SETTINGS-SECTION') {
      parent = parent.nodeType == Node.DOCUMENT_FRAGMENT_NODE ?
          parent.host :
          parent.parentNode;
      if (parent.nodeName == 'SETTINGS-SUBPAGE') {
        // TODO(dpapad): Cast to SettingsSubpageElement here.
        associatedControl = assert(
            parent.associatedControl,
            'An associated control was expected for SETTINGS-SUBPAGE ' +
                parent.pageTitle + ', but was not found.');
      }
    }
    if (parent)
      parent.hiddenBySearch = false;

    // Need to add the search bubble after the parent SETTINGS-SECTION has
    // become visible, otherwise |offsetWidth| returns zero.
    if (associatedControl) {
      cr.search_highlight_utils.highlightControlWithBubble(
          associatedControl, rawQuery);
    }
  }

  /** @abstract */
  class Task {
    /**
     * @param {!settings.SearchRequest} request
     * @param {!Array<!Node>} nodes
     * @param {!function(!Node): void} addTextObserver
     */
    constructor(request, nodes, addTextObserver) {
      /** @protected {!settings.SearchRequest} */
      this.request = request;

      /** @protected {!Array<!Node>} */
      this.nodes = nodes;

      /** @protected {!function(!Node): void} */
      this.addTextObserver = addTextObserver;
    }

    /**
     * @abstract
     * @return {!Promise}
     */
    exec() {}
  }

  class RenderTask extends Task {
    /**
     * A task that takes a <template is="dom-if">...</template> node
     * corresponding to a setting subpage and renders it. A
     * SearchAndHighlightTask is posted for the newly rendered subtree, once
     * rendering is done.
     *
     * @param {!settings.SearchRequest} request
     * @param {!Node} node
     * @param {!function(!Node): void} addTextObserver
     */
    constructor(request, node, addTextObserver) {
      super(request, [node], addTextObserver);
    }

    /** @override */
    exec() {
      const node = this.nodes[0];
      const routePath = node.getAttribute('route-path');
      const subpageTemplate =
          node['_content'].querySelector('settings-subpage');
      subpageTemplate.setAttribute('route-path', routePath);
      assert(!node.if);
      node.if = true;

      return new Promise((resolve, reject) => {
        const parent = node.parentNode;
        parent.async(() => {
          const renderedNode =
              parent.querySelector('[route-path="' + routePath + '"]');
          // Register a SearchAndHighlightTask for the part of the DOM that was
          // just rendered.
          this.request.queue_.addSearchAndHighlightTask(
              new SearchAndHighlightTask(
                  this.request, assert(renderedNode), this.addTextObserver));
          resolve();
        });
      });
    }
  }

  class SearchAndHighlightTask extends Task {
    /**
     * @param {!settings.SearchRequest} request
     * @param {!Node} node
     * @param {!function(!Node): void} addTextObserver
     */
    constructor(request, node, addTextObserver) {
      super(request, [node], addTextObserver);
    }

    /** @override */
    exec() {
      const foundMatches = findAndHighlightMatches_(
          this.request, this.nodes, this.addTextObserver);
      this.request.updateMatches(foundMatches);
      return Promise.resolve();
    }
  }

  class TopLevelSearchTask extends Task {
    /**
     * @param {!settings.SearchRequest} request
     * @param {!Array<!Node>} pages
     */
    constructor(request, pages, addTextObserver) {
      super(request, pages, addTextObserver);
    }

    /** @override */
    exec() {
      findAndRemoveHighlights_(this.nodes);

      const shouldSearch = this.request.regExp !== null;
      this.setSectionsVisibility_(!shouldSearch);
      if (shouldSearch) {
        const foundMatches = findAndHighlightMatches_(
            this.request, this.nodes, this.addTextObserver);
        this.request.updateMatches(foundMatches);
      }

      return Promise.resolve();
    }

    /**
     * @param {boolean} visible
     * @private
     */
    setSectionsVisibility_(visible) {
      this.nodes.forEach(node => {
        const sections = node.querySelectorAll('settings-section');
        for (let i = 0; i < sections.length; i++)
          sections[i].hiddenBySearch = !visible;
      });
    }
  }

  class TaskQueue {
    /** @param {!settings.SearchRequest} request */
    constructor(request) {
      /** @private {!settings.SearchRequest} */
      this.request_ = request;

      /**
       * @private {{
       *   high: !Array<!Task>,
       *   middle: !Array<!Task>,
       *   low: !Array<!Task>
       * }}
       */
      this.queues_;
      this.reset();

      /** @private {?Function} */
      this.onEmptyCallback_ = null;

      /**
       * Whether a task is currently running.
       * @private {boolean}
       */
      this.running_ = false;
    }

    /** Drops all tasks. */
    reset() {
      this.queues_ = {high: [], middle: [], low: []};
    }

    /** @param {!TopLevelSearchTask} task */
    addTopLevelSearchTask(task) {
      this.queues_.high.push(task);
      this.consumePending_();
    }

    /** @param {!SearchAndHighlightTask} task */
    addSearchAndHighlightTask(task) {
      this.queues_.middle.push(task);
      this.consumePending_();
    }

    /** @param {!RenderTask} task */
    addRenderTask(task) {
      this.queues_.low.push(task);
      this.consumePending_();
    }

    /**
     * Registers a callback to be called every time the queue becomes empty.
     * @param {function():void} onEmptyCallback
     */
    onEmpty(onEmptyCallback) {
      this.onEmptyCallback_ = onEmptyCallback;
    }

    /**
     * @return {!Task|undefined}
     * @private
     */
    popNextTask_() {
      return this.queues_.high.shift() || this.queues_.middle.shift() ||
          this.queues_.low.shift();
    }

    /** @private */
    consumePending_() {
      if (this.running_)
        return;

      const task = this.popNextTask_();
      if (!task) {
        this.running_ = false;
        if (this.onEmptyCallback_)
          this.onEmptyCallback_();
        return;
      }

      this.running_ = true;
      window.requestIdleCallback(() => {
        if (!this.request_.canceled) {
          task.exec().then(() => {
            this.running_ = false;
            this.consumePending_();
          });
        }
        // Nothing to do otherwise. Since the request corresponding to this
        // queue was canceled, the queue is disposed along with the request.
      });
    }
  }

  class SearchRequest {
    /**
     * @param {string} rawQuery
     * @param {!Array<!HTMLElement>} roots
     * @param {!function(!Node): void} addTextObserver
     */
    constructor(rawQuery, roots, addTextObserver) {
      /** @private {string} */
      this.rawQuery_ = rawQuery;

      /** @private {!Array<!HTMLElement>} */
      this.roots_ = roots;

      /** @type {?RegExp} */
      this.regExp = this.generateRegExp_();

      /**
       * Whether this request was canceled before completing.
       * @type {boolean}
       */
      this.canceled = false;

      /** @private {boolean} */
      this.foundMatches_ = false;

      /** @type {!PromiseResolver} */
      this.resolver = new PromiseResolver();

      /** @private {!TaskQueue} */
      this.queue_ = new TaskQueue(this);
      this.queue_.onEmpty(() => {
        this.resolver.resolve(this);
      });

      /** @private {!function(!Node): void} */
      this.addTextObserver_ = addTextObserver;
    }

    /**
     * Fires this search request.
     */
    start() {
      this.queue_.addTopLevelSearchTask(
          new TopLevelSearchTask(this, this.roots_, this.addTextObserver_));
    }

    /**
     * @return {?RegExp}
     * @private
     */
    generateRegExp_() {
      let regExp = null;

      // Generate search text by escaping any characters that would be
      // problematic for regular expressions.
      const searchText = this.rawQuery_.trim().replace(SANITIZE_REGEX, '\\$&');
      if (searchText.length > 0)
        regExp = new RegExp(`(${searchText})`, 'i');

      return regExp;
    }

    /**
     * @param {string} rawQuery
     * @return {boolean} Whether this SearchRequest refers to an identical
     *     query.
     */
    isSame(rawQuery) {
      return this.rawQuery_ == rawQuery;
    }

    /**
     * Updates the result for this search request.
     * @param {boolean} found
     */
    updateMatches(found) {
      this.foundMatches_ = this.foundMatches_ || found;
    }

    /** @return {!settings.SearchResult} */
    result() {
      return /** @type {!settings.SearchResult} */ ({
        canceled: this.canceled,
        didFindMatches: this.foundMatches_,
        rawQuery: this.rawQuery_,
        wasClearSearch: this.isSame(''),
      });
    }
  }

  /** @type {!RegExp} */
  const SANITIZE_REGEX = /[-[\]{}()*+?.,\\^$|#\s]/g;

  /** @interface */
  class SearchManagerObserver {
    onSearchStart() {}

    /** @param {!settings.SearchResult} searchResult */
    onSearchComplete(searchResult) {}
  }

  /** @interface */
  class SearchManager {
    /**
     * @param {string} text The text to search for.
     * @param {!Array<!Node>} pages
     * @return {!Promise<!settings.SearchResult>} A signal indicating that
     *     searching finished.
     */
    search(text, pages) {}

    /** @param {!settings.SearchManagerObserver} observer */
    registerObserver(observer) {}
  }

  /** @implements {SearchManager} */
  class SearchManagerImpl {
    constructor() {
      /** @private {!Set<!settings.SearchRequest>} */
      this.activeRequests_ = new Set();

      /** @private {?string} */
      this.lastSearchedText_ = null;

      /** @private {!Array<!settings.SearchManagerObserver>} */
      this.observers_ = [];

      /** @private {!Set<!MutationObserver>} */
      this.textObservers_ = new Set();
    }

    /**
     * @param {!function(): void} redoSearch
     * @return {!function(!Node): void}
     * @private
     */
    getAddTextObserverFunction_(redoSearch) {
      this.textObservers_.forEach(observer => {
        observer.disconnect();
      });
      this.textObservers_.clear();
      return node => {
        const observer = new MutationObserver(mutations => {
          const oldValue = mutations[0].oldValue.trim();
          const newValue = mutations[0].target.nodeValue.trim();
          if (oldValue != newValue)
            redoSearch();
        });
        observer.observe(
            node, {characterData: true, characterDataOldValue: true});
        this.textObservers_.add(observer);
      };
    }

    /** @override */
    search(text, pages) {
      // Cancel any pending requests if a request with different text is
      // submitted.
      if (text != this.lastSearchedText_) {
        this.activeRequests_.forEach(function(request) {
          request.canceled = true;
          request.resolver.resolve(request);
        });
        this.activeRequests_.clear();
      }

      this.lastSearchedText_ = text;

      const addTextObserver = this.getAddTextObserverFunction_(() => {
        this.search(text, pages);
      });
      const request = new SearchRequest(text, pages, addTextObserver);
      this.activeRequests_.add(request);
      request.start();
      this.observers_.forEach(observer => {
        observer.onSearchStart.call(observer);
      });
      return request.resolver.promise.then(() => {
        // Stop tracking requests that finished.
        this.activeRequests_.delete(request);
        this.observers_.forEach(observer => {
          observer.onSearchComplete.call(observer, request.result());
        });
        return request.result();
      });
    }

    /** @override */
    registerObserver(observer) {
      this.observers_.push(observer);
    }
  }
  cr.addSingletonGetter(SearchManagerImpl);

  /** @return {!SearchManager} */
  function getSearchManager() {
    return SearchManagerImpl.getInstance();
  }

  /**
   * Sets the SearchManager singleton instance, useful for testing.
   * @param {!SearchManager} searchManager
   */
  function setSearchManagerForTesting(searchManager) {
    SearchManagerImpl.instance_ = searchManager;
  }

  return {
    getSearchManager: getSearchManager,
    setSearchManagerForTesting: setSearchManagerForTesting,
    SearchManagerObserver: SearchManagerObserver,
    SearchRequest: SearchRequest,
  };
});
