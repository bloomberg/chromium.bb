// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: cr.js
// require: cr/ui.js
// require: cr/ui/tree.js

cr.define('chrome.sync', function() {
  /**
   * Gets all children of the given node and passes it to the given
   * callback.
   * @param {string} id The id whose children we want.
   * @param {function(Array.<!Object>)} callback The callback to call
   *     with the list of children summaries.
   */
  function getSyncNodeChildrenSummaries(id, callback) {
    var timer = chrome.sync.makeTimer();
    chrome.sync.getChildNodeIds(id, function(childNodeIds) {
      console.debug('getChildNodeIds took ' +
                    timer.elapsedSeconds + 's to retrieve ' +
                    childNodeIds.length + ' ids');
      timer = chrome.sync.makeTimer();
      chrome.sync.getNodeSummariesById(
          childNodeIds, function(childrenSummaries) {
        console.debug('getNodeSummariesById took ' +
                      timer.elapsedSeconds + 's to retrieve summaries for ' +
                      childrenSummaries.length + ' nodes');
        callback(childrenSummaries);
      });
    });
  }

  /**
   * Creates a new sync node tree item.
   * @param {{id: string, title: string, isFolder: boolean}}
   *     nodeSummary The nodeSummary object for the node (as returned
   *     by chrome.sync.getNodeSummariesById()).
   * @constructor
   * @extends {cr.ui.TreeItem}
   */
  var SyncNodeTreeItem = function(nodeSummary) {
    var treeItem = new cr.ui.TreeItem({
      id_: nodeSummary.id
    });
    treeItem.__proto__ = SyncNodeTreeItem.prototype;

    treeItem.label = nodeSummary.title;
    if (nodeSummary.isFolder) {
      treeItem.mayHaveChildren_ = true;

      // Load children asynchronously on expand.
      // TODO(akalin): Add a throbber while loading?
      treeItem.triggeredLoad_ = false;
      treeItem.addEventListener('expand',
                                treeItem.handleExpand_.bind(treeItem));
    } else {
      treeItem.classList.add('leaf');
    }
    return treeItem;
  };

  SyncNodeTreeItem.prototype = {
    __proto__: cr.ui.TreeItem.prototype,

    /**
     * Retrieves the details for this node.
     * @param {function(Object)} callback The callback that will be
     *    called with the node details, or null if it could not be
     *    retrieved.
     */
    getDetails: function(callback) {
      chrome.sync.getNodeDetailsById([this.id_], function(nodeDetails) {
        callback(nodeDetails[0] || null);
      });
    },

    handleExpand_: function(event) {
      if (!this.triggeredLoad_) {
        getSyncNodeChildrenSummaries(this.id_, this.addChildNodes_.bind(this));
        this.triggeredLoad_ = true;
      }
    },

    /**
     * Adds children from the list of children summaries.
     * @param {Array.<{id: string, title: string, isFolder: boolean}>}
     *    childrenSummaries The list of children summaries with which
     *    to create the child nodes.
     */
    addChildNodes_: function(childrenSummaries) {
      var timer = chrome.sync.makeTimer();
      for (var i = 0; i < childrenSummaries.length; ++i) {
        var childTreeItem = new SyncNodeTreeItem(childrenSummaries[i]);
        this.add(childTreeItem);
      }
      console.debug('adding ' + childrenSummaries.length +
                    ' children took ' + timer.elapsedSeconds + 's');
    }
  };

  /**
   * Updates the node detail view with the details for the given node.
   * @param {!Object} nodeDetails The details for the node we want
   *     to display.
   */
  function updateNodeDetailView(nodeDetails) {
    var nodeBrowser = document.getElementById('node-browser');
    // TODO(akalin): Write a nicer detail viewer.
    nodeDetails.entry = JSON.stringify(nodeDetails.entry, null, 2);
    jstProcess(new JsEvalContext(nodeDetails), nodeBrowser);
  }

  /**
   * Creates a new sync node tree.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.Tree}
   */
  var SyncNodeTree = cr.ui.define('tree');

  SyncNodeTree.prototype = {
    __proto__: cr.ui.Tree.prototype,

    decorate: function() {
      cr.ui.Tree.prototype.decorate.call(this);
      this.addEventListener('change', this.handleChange_.bind(this));
      chrome.sync.getRootNodeDetails(this.makeRoot_.bind(this));
    },

    /**
     * Creates the root of the tree.
     * @param {{id: string, title: string, isFolder: boolean}}
     *    rootNodeSummary The summary info for the root node.
     */
    makeRoot_: function(rootNodeSummary) {
      // The root node usually doesn't have a title.
      rootNodeSummary.title = rootNodeSummary.title || 'Root';
      var rootTreeItem = new SyncNodeTreeItem(rootNodeSummary);
      this.add(rootTreeItem);
    },

    handleChange_: function(event) {
      if (this.selectedItem) {
        this.selectedItem.getDetails(updateNodeDetailView);
      }
    }
  };

  function decorateSyncNodeBrowser(syncNodeBrowser) {
    cr.ui.decorate(syncNodeBrowser, SyncNodeTree);
  }

  // This is needed because JsTemplate (which is needed by
  // updateNodeDetailView) is loaded at the end of the file after
  // everything else.
  //
  // TODO(akalin): Remove dependency on JsTemplate and get rid of
  // this.
  var domLoaded = false;
  var pendingSyncNodeBrowsers = [];
  function decorateSyncNodeBrowserAfterDOMLoad(id) {
    var e = document.getElementById(id);
    if (domLoaded) {
      decorateSyncNodeBrowser(e);
    } else {
      pendingSyncNodeBrowsers.push(e);
    }
  }

  document.addEventListener('DOMContentLoaded', function() {
    for (var i = 0; i < pendingSyncNodeBrowsers.length; ++i) {
      decorateSyncNodeBrowser(pendingSyncNodeBrowsers[i]);
    }
    domLoaded = true;
  });

  return {
    decorateSyncNodeBrowser: decorateSyncNodeBrowserAfterDOMLoad
  };
});
