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
    name: 'Drive',
    navItem: '#navigation-list-1',
    treeItem: TREEITEM_DRIVE
  },
  A: {
    contents: [ENTRIES.directoryB.getExpectedRow()],
    name: 'A',
    navItem: '#navigation-list-4',
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
    navItem: '#navigation-list-4',
    treeItem: TREEITEM_C
  },
  D: {
    contents: [ENTRIES.directoryE.getExpectedRow()],
    name: 'D',
    navItem: '#navigation-list-3',
    treeItem: TREEITEM_D
  },
  E: {
    contents: [ENTRIES.directoryF.getExpectedRow()],
    name: 'E',
    treeItem: TREEITEM_E
  }
};

/**
 * Opens two file manager windows.
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
}

/**
 * Expands tree item on the directory tree by clicking expand icon.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory whose tree item should be expanded.
 * @return {Promise} Promise fulfilled on success.
 */
function expandTreeItem(windowId, directory) {
  return waitForElement(
      windowId, directory.treeItem + EXPAND_ICON).then(function() {
    return callRemoteTestUtil(
        'fakeMouseClick', windowId, [directory.treeItem + EXPAND_ICON]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElement(windowId, directory.treeItem + EXPANDED_SUBTREE);
  });
}

/**
 * Expands whole directory tree.
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
}

/**
 * Makes |directory| the current directory.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory which should be a current directory.
 * @return {Promise} Promise fulfilled on success.
 */
function navigateToDirectory(windowId, directory) {
  return waitForElement(
      windowId, directory.treeItem + VOLUME_ICON).then(function() {
    return callRemoteTestUtil(
        'fakeMouseClick', windowId, [directory.treeItem + VOLUME_ICON]);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForFiles(windowId, directory.contents);
  });
}

/**
 * Creates folder shortcut to |directory|.
 * The current directory must be a parent of the |directory|.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory of shortcut to be created.
 * @return {Promise} Promise fulfilled on success.
 */
function createShortcut(windowId, directory) {
  return callRemoteTestUtil(
      'selectFile', windowId, [directory.name]).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElement(windowId, ['.table-row[selected]']);
  }).then(function() {
    return callRemoteTestUtil(
        'fakeMouseRightClick', windowId, ['.table-row[selected]']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElement(windowId, '#file-context-menu:not([hidden])');
  }).then(function() {
    return waitForElement(windowId, '[command="#create-folder-shortcut"]');
  }).then(function() {
    return callRemoteTestUtil(
        'fakeMouseClick', windowId, ['[command="#create-folder-shortcut"]']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElement(windowId, directory.navItem);
  });
}

/**
 * Removes folder shortcut to |directory|.
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
    return waitForElement(windowId, '[command="#remove-folder-shortcut"]');
  }).then(function() {
    return callRemoteTestUtil(
        'fakeMouseClick', windowId, ['[command="#remove-folder-shortcut"]']);
  }).then(function(result) {
    chrome.test.assertTrue(result);
    return waitForElementLost(windowId, directory.navItem);
  });
}

/**
 * Waits until the current directory become |currentDir| and folder shortcut to
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
 * Clicks folder shortcut to |directory|.
 * @param {string} windowId ID of target window.
 * @param {Object} directory Directory whose shortcut will be clicked.
 * @return {Promise} Promise fullfilled with result of fakeMouseClick.
 */
function clickShortcut(windowId, directory) {
  return waitForElement(windowId, directory.navItem).then(function() {
    return callRemoteTestUtil('fakeMouseClick', windowId, [directory.navItem]);
  });
}

/**
 * Creates some shortcuts and traverse them and some other directories.
 */
testcase.traverseFolderShortcuts = function() {
  var windowId;
  StepsRunner.run([
    // Set up each window.
    function() {
      addEntries(['drive'], ENTRY_SET, this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      openNewWindow(null, RootPath.DRIVE).then(this.next);
    },
    function(inWindowId) {
      windowId = inWindowId;
      waitForElement(windowId, '#detail-table').then(this.next);
    },
    function() {
      expandDirectoryTree(windowId).then(this.next);
    },
    function() {
      waitForFiles(windowId, DIRECTORY.Drive.contents).then(this.next);
    },

    // Create shortcut to D
    function() {
      createShortcut(windowId, DIRECTORY.D).then(this.next);
    },

    // Create sortcut to C
    function() {
      navigateToDirectory(windowId, DIRECTORY.B).then(this.next);
    },
    function() {
      createShortcut(windowId, DIRECTORY.C).then(this.next);
    },

    // Navigate to E.
    // Current directory should be E.
    // Shortcut to D should be selected.
    function() {
      navigateToDirectory(windowId, DIRECTORY.E).then(this.next);
    },
    function() {
      expectSelection(windowId, DIRECTORY.E, DIRECTORY.D).then(this.next);
    },

    // Click shortcut to drive.
    // Current directory should be Drive root.
    // Shortcut to Drive root should be selected.
    function() {
      clickShortcut(windowId, DIRECTORY.Drive).then(this.next);
    },
    function() {
      expectSelection(
          windowId, DIRECTORY.Drive, DIRECTORY.Drive).then(this.next);
    },

    // Press Ctrl+4 to select 4th shortcut.
    // Current directory should be D.
    // Shortcut to C should be selected.
    function() {
      callRemoteTestUtil('fakeKeyDown', windowId,
          ['#file-list', 'U+0034', true], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId, DIRECTORY.D, DIRECTORY.D).then(this.next);
    },

    // Press UP to select 3rd shortcut.
    // Current directory should be C.
    // Shortcut to C should be selected.
    function() {
      callRemoteTestUtil('fakeKeyDown', windowId,
          ['#navigation-list', 'Up', false], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      expectSelection(windowId, DIRECTORY.C, DIRECTORY.C).then(this.next);
    },

    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Adds and removes shortcuts from other window and check if the active
 * directories and selected navigation items are correct.
 */
testcase.addRemoveFolderShortcuts = function() {
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
      expandDirectoryTree(windowId1).then(this.next);
    },
    function() {
      waitForFiles(windowId1, DIRECTORY.Drive.contents).then(this.next);
    },
    function() {
      waitForFiles(windowId2, DIRECTORY.Drive.contents).then(this.next);
    },

    // Create shortcut to D
    function() {
      createShortcut(windowId1, DIRECTORY.D).then(this.next);
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

    // Create shortcut to A in another window.
    function() {
      createShortcut(windowId2, DIRECTORY.A).then(this.next);
    },

    // The index of shortcut to D is changed.
    // Current directory should remain E.
    // Shortcut to D should keep selected.
    function() {
      expectSelection(windowId1, DIRECTORY.E, DIRECTORY.D).then(this.next);
    },

    // Remove shortcut to D in another window.
    function() {
      removeShortcut(windowId2, DIRECTORY.D).then(this.next);
    },

    // Current directory should remain E.
    // Shortcut to Drive root should be selected.
    function() {
      expectSelection(windowId1, DIRECTORY.E, DIRECTORY.Drive).then(this.next);
    },

    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

