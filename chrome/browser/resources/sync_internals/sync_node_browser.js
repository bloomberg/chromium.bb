// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: cr.js
// require: cr/ui.js
// require: cr/ui/tree.js

cr.define('chrome.sync', function() {
  // Allow platform specific CSS rules.
  //
  // TODO(akalin): BMM and options page does something similar, too.
  // Move this to util.js.
  if (cr.isWindows)
    document.documentElement.setAttribute('os', 'win');

  // TODO(akalin): Create SyncNodeTree/SyncNodeTreeItem classes that
  // hide all these details.

  /**
   * Gets all children of the given node and passes it to the given
   * callback.
   * @param {Object} nodeInfo The info for the node whose children we
   *     want.
   * @param {Function} callback The callback to call with the list of
   *     children.
   */
  function getSyncNodeChildren(nodeInfo, callback) {
    var children = [];
    function processChildInfo(childNodeInfo) {
      if (!childNodeInfo) {
        callback(children);
        return;
      }
      children.push(childNodeInfo);
      chrome.sync.getNodeById(childNodeInfo.successorId, processChildInfo);
    };
    chrome.sync.getNodeById(nodeInfo.firstChildId, processChildInfo);
  }

  /**
   * Makes a tree item from the given node info.
   * @param {dictionary} nodeInfo The node info to create the tree
   *    item from.
   * @return {cr.ui.TreeItem} The created tree item.
   */
  function makeNodeTreeItem(nodeInfo) {
    var treeItem = new cr.ui.TreeItem({
      label: nodeInfo.title,
      detail: nodeInfo
    });

    if (nodeInfo.isFolder) {
      treeItem.mayHaveChildren_ = true;

      // Load children asynchronously on expand.
      // TODO(akalin): Add a throbber while loading?
      treeItem.triggeredLoad_ = false;
      treeItem.addEventListener('expand', function(event) {
        if (!treeItem.triggeredLoad_) {
          getSyncNodeChildren(nodeInfo, function(children) {
            for (var i = 0; i < children.length; ++i) {
              var childTreeItem = makeNodeTreeItem(children[i]);
              treeItem.add(childTreeItem);
            }
          });
          treeItem.triggeredLoad_ = true;
        }
      });
    } else {
      treeItem.classList.add('leaf');
    }
    return treeItem;
  }

  /**
   * Updates the node detail view with the info for the given node.
   * @param {dictionary} nodeInfo The info for the node we want to
   *     display.
   */
  function updateNodeDetailView(nodeInfo) {
    var nodeBrowser = document.getElementById('node-browser');
    // TODO(akalin): Get rid of this hack.
    if (typeof nodeInfo.entry != 'string')
      nodeInfo.entry = JSON.stringify(nodeInfo.entry, null, 2);
    jstProcess(new JsEvalContext(nodeInfo), nodeBrowser);
  }

  function decorateSyncNodeBrowser(syncNodeBrowser) {
    cr.ui.decorate(syncNodeBrowser, cr.ui.Tree);

    syncNodeBrowser.addEventListener('change', function(event) {
      if (syncNodeBrowser.selectedItem)
        updateNodeDetailView(syncNodeBrowser.selectedItem.detail);
    });

    chrome.sync.getRootNode(function(rootNodeInfo) {
      var rootTreeItem = makeNodeTreeItem(rootNodeInfo);
      rootTreeItem.label = 'Root';
      syncNodeBrowser.add(rootTreeItem);
    });
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
