// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Constants for selectors.
 */
var TREEITEM_DRIVE = '#directory-tree > div:nth-child(1) ';
var TREEITEM_A = TREEITEM_DRIVE + '> .tree-children > div:nth-child(1) ';
var TREEITEM_B = TREEITEM_A + '> .tree-children > div:nth-child(1) ';
var TREEITEM_C = TREEITEM_B + '> .tree-children > div:nth-child(1) ';
var TREEITEM_D = TREEITEM_DRIVE + '> .tree-children > div:nth-child(2) ';
var TREEITEM_E = TREEITEM_D + '> .tree-children > div:nth-child(1) ';
var EXPAND_ICON = '> .tree-row > .expand-icon';
var VOLUME_ICON = '> .tree-row > .volume-icon';
var EXPANDED_SUBTREE = '> .tree-children[expanded]';

/**
 * Entry set which is used for this test.
 * @type {Array.<TestEntryInfo>}
 * @const
 */
var ENTRY_SET = [
  ENTRIES.directoryA,
  ENTRIES.directoryB,
  ENTRIES.directoryC,
  ENTRIES.directoryD,
  ENTRIES.directoryE,
  ENTRIES.directoryF
];

/**
 * Constants for each folders.
 * @type {Array.<Object>}
 * @const
 */
var DIRECTORY = {
  Drive: {
    contents: [ENTRIES.directoryA.getExpectedRow(),
               ENTRIES.directoryD.getExpectedRow()],
    navItem: '#navigation-list-1',
    treeItem: TREEITEM_DRIVE
  },
  A: {
    contents: [ENTRIES.directoryB.getExpectedRow()],
    name: 'A',
    navItem: '#navigation-list-5',
    treeItem: TREEITEM_A
  },
  B: {
    contents: [ENTRIES.directoryC.getExpectedRow()],
    name: 'B',
    treeItem: TREEITEM_B
  },
  C: {
    contents: [],
    name: 'C',
    navItem: '#navigation-list-3',
    treeItem: TREEITEM_C
  },
  D: {
    contents: [ENTRIES.directoryE.getExpectedRow()],
    name: 'D',
    navItem: '#navigation-list-4',
    treeItem: TREEITEM_D
  },
  E: {
    contents: [ENTRIES.directoryF.getExpectedRow()],
    name: 'E',
    treeItem: TREEITEM_E
  }
};

/**
 * Open two file manager windows.
 * @return {Promise} Promise fulfilled with an array containing two window IDs.
 */
function openWindows() {
  return Promise.all([
    openNewWindow(null, RootPath.DRIVE),
    openNewWindow(null, RootPath.DRIVE)
  ]).then(function(windowIds) {
    return Promise.all([
      waitForElement(windowIds[0], '#detail-table'),
      waitForElement(windowIds[1], '#detail-table')
    ]).then(function() {
      return windowIds;
    });
  });
};

/**
 * Expand tree item on the directory tree by clicking expand icon.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory whose tree item should be expanded.
 * @return {Promise} Promise fulfilled on success.
 */
function expandTreeItem(windowId, directory) {
  return callRemoteTestUtil(
      'fakeMouseClick',
      windowId,
      [directory.treeItem + EXPAND_ICON]).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElement(windowId, directory.treeItem + EXPANDED_SUBTREE);
  });
}

/**
 * Expand whole directory tree.
 * @param {string} windowId ID of target window.
 * @return {Promise} Promise fulfilled on success.
 */
function expandDirectoryTree(windowId) {
  return expandTreeItem(windowId, DIRECTORY.Drive).then(function() {
    return expandTreeItem(windowId, DIRECTORY.A);
  }).then(function() {
    return expandTreeItem(windowId, DIRECTORY.B);
  }).then(function() {
    return expandTreeItem(windowId, DIRECTORY.D);
  });
};

/**
 * Make |directory| the current directory.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory which should be a current directory.
 * @return {Promise} Promise fulfilled on success.
 */
function navigateToDirectory(windowId, directory) {
  return callRemoteTestUtil(
      'fakeMouseClick',
      windowId,
      [directory.treeItem + VOLUME_ICON]).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForFiles(windowId, directory.contents);
  });
}

/**
 * Create folder shortcut to |directory|.
 * The current directory must be a parent of the |directory|.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory of shortcut to be created.
 * @return {Promise} Promise fulfilled on success.
 */
function createShortcut(windowId, directory) {
  return callRemoteTestUtil(
      'selectFile', windowId, [directory.name]).then(function(result) {
    chrome.test.assertTrue(result);
    return callRemoteTestUtil(
        'fakeMouseRightClick', windowId, ['.table-row[selected]']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElement(windowId, '#file-context-menu:not([hidden])');
  }).then(function() {
    return callRemoteTestUtil(
        'fakeMouseClick', windowId, ['[command="#create-folder-shortcut"]']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElement(windowId, directory.navItem);
  });
}

/**
 * Remove folder shortcut to |directory|.
 * The current directory must be a parent of the |directory|.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory of shortcut ot be removed.
 * @return {Promise} Promise fullfilled on success.
 */
function removeShortcut(windowId, directory) {
  return callRemoteTestUtil(
      'fakeMouseRightClick',
      windowId,
      [directory.navItem]).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElement(windowId, '#roots-context-menu:not([hidden])');
  }).then(function() {
    return callRemoteTestUtil(
        'fakeMouseClick', windowId, ['[command="#remove-folder-shortcut"]']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElementLost(windowId, directory.navItem);
  });
}

/**
 * Wait until the current directory become |currentDir| and folder shortcut to
 * |shortcutDir| is selected.
 * @param {string} windowId ID of target window.
 * @param {Object} currentDir Directory which should be a current directory.
 * @param {Object} shortcutDir Directory whose shortcut should be selected.
 * @return {Promise} Promise fullfilled on success.
 */
function expectSelection(windowId, currentDir, shortcutDir) {
  return waitForFiles(windowId, currentDir.contents).then(function() {
    return waitForElement(windowId, shortcutDir.navItem + '[selected]');
  });
}

/**
 * Click folder shortcut to |directory|.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory whose shortcut will be clicked.
 * @return {Promise} Promise fullfilled with result of fakeMouseClick.
 */
function clickShortcut(windowId, directory) {
  return callRemoteTestUtil('fakeMouseClick', windowId, [directory.navItem]);
}

testcase.traverseFolderShortcuts = function() {
  var windowId1;
  var windowId2;
  StepsRunner.run([
    // Set up each window.
    function() {
      addEntries(['drive'], ENTRY_SET, this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      openWindows().then(this.next);
    },
    function(windowIds) {
      windowId1 = windowIds[0];
      windowId2 = windowIds[1];
      waitForFiles(windowId1, DIRECTORY.Drive.contents).then(this.next);
    },
    function() {
      expandDirectoryTree(windowId1).then(this.next);
    },
    function() {
      expandDirectoryTree(windowId2).then(this.next);
    },

    // Create sortcut to C
    function() {
      navigateToDirectory(windowId1, DIRECTORY.B).then(this.next);
    },
    function() {
      createShortcut(windowId1, DIRECTORY.C).then(this.next);
    },

    // Create shortcut to D
    function() {
      navigateToDirectory(windowId1, DIRECTORY.Drive).then(this.next);
    },
    function() {
      createShortcut(windowId1, DIRECTORY.D).then(this.next);
    },

    // Navigate to E.
    // Current directory should be E.
    // Shortcut to D should be selected.
    function() {
      navigateToDirectory(windowId1, DIRECTORY.E).then(this.next);
    },
    function() {
      expectSelection(windowId1, DIRECTORY.E, DIRECTORY.D).then(this.next);
    },

    // Navigate to Drive root.
    // Current directory should be Drive root.
    // Shortcut to Drive root should be selected.
    function() {
      navigateToDirectory(windowId1, DIRECTORY.Drive).then(this.next);
    },
    function() {
      expectSelection(
          windowId1, DIRECTORY.Drive, DIRECTORY.Drive).then(this.next);
    },

    // Navigate to E.
    // Current directory should be E.
    // Shortcut to E should be selected.
    function() {
      navigateToDirectory(windowId1, DIRECTORY.E).then(this.next);
    },
    function() {
      expectSelection(windowId1, DIRECTORY.E, DIRECTORY.D).then(this.next);
    },

    // Create shortcut to A in another file manager window.
    function() {
      navigateToDirectory(windowId2, DIRECTORY.Drive).then(this.next);
    },
    function() {
      createShortcut(windowId2, DIRECTORY.A).then(this.next);
    },

    // The index of shortcut to D is changed.
    // Current directory should remain E.
    // Shortcut to D should keep selected.
    function() {
      expectSelection(windowId1, DIRECTORY.E, DIRECTORY.D).then(this.next);
    },

    // Click shortcut to D which is ascendant of current directory.
    // Current directory should be D.
    // Shortcut to D should keep selected.
    function() {
      clickShortcut(windowId1, DIRECTORY.D).then(this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId1, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },

    // Click shortcut to C which don't have ascendant-descentant relation with
    // current directory.
    // Current directory should be C.
    // Shortcut to C should be selected.
    function() {
      clickShortcut(windowId1, DIRECTORY.C).then(this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId1, DIRECTORY.C, DIRECTORY.C).then(this.next);
    },

    // Navigate to B which is ascendant of C and descendant of A.
    // Current directory should be B.
    // Shortcut to A should be selected.
    function() {
      navigateToDirectory(windowId1, DIRECTORY.B).then(this.next);
    },
    function() {
      expectSelection(windowId1, DIRECTORY.B, DIRECTORY.A).then(this.next);
    },

    // Navigate to A which is ascendant of B and C.
    // Current directory should be A.
    // Shortcut to A should be selected.
    function() {
      navigateToDirectory(windowId1, DIRECTORY.A).then(this.next);
    },
    function() {
      expectSelection(windowId1, DIRECTORY.A, DIRECTORY.A).then(this.next);
    },

    // Remove shortcut to A in another window.
    function() {
      navigateToDirectory(windowId2, DIRECTORY.Drive).then(this.next);
    },
    function() {
      removeShortcut(windowId2, DIRECTORY.A).then(this.next);
    },

    // Current directory should remain A.
    // Shortcut to Drive root should be selected.
    function() {
      expectSelection(windowId1, DIRECTORY.A, DIRECTORY.Drive).then(this.next);
    },

    // Press Ctrl+1 to select 1st shortcut.
    // Current directory should be Drive root.
    // Shortcut to Drive root should be selected.
    function() {
      callRemoteTestUtil('fakeKeyDown', windowId1,
          ['#file-list', 'U+0031', true], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(
          windowId1, DIRECTORY.Drive, DIRECTORY.Drive).then(this.next);
    },

    // Press Ctrl+4 to select 4th shortcut.
    // Current directory should be D.
    // Shortcut to C should be selected.
    function() {
      callRemoteTestUtil('fakeKeyDown', windowId1,
          ['#file-list', 'U+0034', true], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId1, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },

    // Press UP to select 3rd shortcut.
    // Current directory should be C.
    // Shortcut to C should be selected.
    function() {
      callRemoteTestUtil('fakeKeyDown', windowId1,
          ['#navigation-list', 'Up', false], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId1, DIRECTORY.C, DIRECTORY.C).then(this.next);
    },

    // Press DOWN to select 4th shortcut.
    // Current directory should be D.
    // Shortcut to D should be selected.
    function() {
      callRemoteTestUtil('fakeKeyDown', windowId1,
          ['#navigation-list', 'Down', false], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId1, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },

    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
