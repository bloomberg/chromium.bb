// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testTreeChangedObserverForCreatingNode() {
    chrome.automation.addTreeChangeObserver(function(change) {
      if (change.type == "subtreeCreated" && change.target.name == "New") {
        chrome.test.succeed();
      }
    });

    var addButton = rootNode.find({ attributes: { name: 'Add' }});
    addButton.doDefault();
  },

  function testTreeChangedObserverForRemovingNode() {
    chrome.automation.addTreeChangeObserver(function(change) {
      if (change.type == "nodeRemoved" && change.target.role == "listItem") {
        chrome.test.succeed();
      }
    });

    var removeButton = rootNode.find({ attributes: { name: 'Remove' }});
    removeButton.doDefault();
  }
];

setUpAndRunTests(allTests, 'tree_change.html');
