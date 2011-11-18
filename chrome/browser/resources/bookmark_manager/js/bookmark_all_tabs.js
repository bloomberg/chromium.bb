// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const BookmarkTree = bmm.BookmarkTree;
const TreeItem = cr.ui.TreeItem;

// Delay time (ms) of updating hash after changing current folder.
const NAVIGATE_HASH_UPDATE_DELAY = 250;
// Delay time (ms) of selecting new folder after creation of it.
const NEWFOLDER_SELECTION_DELAY = 50;

// Sometimes the extension API is not initialized.
if (!chrome.bookmarks)
  console.error('Bookmarks extension API is not available');

cr.enablePlatformSpecificCSSRules();

/**
 * The local strings object which is used to do the translation.
 * @type {!LocalStrings}
 */
var localStrings = new LocalStrings;

// Get the localized strings from the backend.
chrome.experimental.bookmarkManager.getStrings(function(data) {
  // The strings may contain & which we need to strip.
  for (var key in data) {
    data[key] = data[key].replace(/&/, '');
  }

  localStrings.templateData = data;
  i18nTemplate.process(document, data);
});

/**
 * The id of the bookmark root.
 * @type {number}
 */
const ROOT_ID = '0';

var searchTreeItem = new TreeItem({
  icon: 'images/bookmark_manager_search.png',
  bookmarkId: 'q='
});
bmm.treeLookup[searchTreeItem.bookmarkId] = searchTreeItem;

var recentTreeItem = new TreeItem({
  icon: 'images/bookmark_manager_recent.png',
  bookmarkId: 'recent'
});
bmm.treeLookup[recentTreeItem.bookmarkId] = recentTreeItem;

BookmarkTree.decorate(tree);
tree.reload();
bmm.addBookmarkModelListeners();

tree.addEventListener('change', function() {
  if (tree.selectedItem)
    navigateTo(tree.selectedItem.bookmarkId);
});

/**
 * Add an event listener to a node that will remove itself after firing once.
 * @param {!Element} node The DOM node to add the listener to.
 * @param {string} name The name of the event listener to add to.
 * @param {function(Event)} handler Function called when the event fires.
 */
function addOneShotEventListener(node, name, handler) {
  var f = function(e) {
    handler(e);
    node.removeEventListener(name, f);
  };
  node.addEventListener(name, f);
}

/**
 * Updates the location hash to reflect the current state of the application.
 */
function updateHash() {
  window.location.hash = tree.selectedItem.bookmarkId;
}

/**
 * Navigates to a bookmark ID.
 * @param {string} id The ID to navigate to.
 * @param {boolean=} opt_updateHashNow Whether to immediately update the
 *     location.hash. If false then it is updated in a timeout.
 */
function navigateTo(id, opt_updateHashNow) {
  // Update the location hash using a timer to prevent reentrancy. This is how
  // often we add history entries and the time here is a bit arbitrary but was
  // picked as the smallest time a human perceives as instant.

  clearTimeout(navigateTo.timer_);
  if (opt_updateHashNow)
    updateHash();
  else
    navigateTo.timer_ = setTimeout(updateHash, NAVIGATE_HASH_UPDATE_DELAY);

  updateParentId(id);
}

/**
 * Checks disabilities of the buttons.
 */
function checkDisabilities() {
  var selected = tree.selectedItem;
  var newNameText = document.querySelector('#new-name').value;

  document.querySelector('#new-folder').disabled = !selected;
  document.querySelector('#save').disabled = !selected || !newNameText;
}
document.querySelector('#new-name')
    .addEventListener('input', checkDisabilities);
tree.addEventListener('change', checkDisabilities);
checkDisabilities();

/**
 * Callback for the new folder command. This creates a new folder and starts
 * a rename of it.
 */
function newFolder() {
  var parentId = tree.selectedItem.bookmarkId;
  chrome.bookmarks.create({
    title: localStrings.getString('new_folder_name'),
    parentId: parentId
  }, function(newNode) {
    // This callback happens before the event that triggers the tree/list to
    // get updated so delay the work so that the tree/list gets updated first.
    setTimeout(function() {
      var newItem = bmm.treeLookup[newNode.id];
      tree.selectedItem = newItem;
      newItem.editing = true;
    }, NEWFOLDER_SELECTION_DELAY);
  });
}

/**
 * Close window without bookmark all tabs.
 */
function cancel() {
  window.close();
}

/**
 * Updates the parent ID of the bookmark list and selects the correct tree item.
 * @param {string} id The id.
 */
function updateParentId(id) {
  if (id in bmm.treeLookup)
    tree.selectedItem = bmm.treeLookup[id];
}

/**
 * Stores the information of all the tabs when window opens.
 * @type {array of tab}
 */
var allTabs = {};
chrome.windows.getCurrent(function (win) {
  chrome.tabs.getAllInWindow(win.id, function (tabs) {
    allTabs = tabs;
  });
});

/**
 * Creates a folder and Adds tabs as bookmarks under the folder.
 */
function bookmarkAllTabs() {
  var parentBookmark = tree.selectedItem;
  var newNameTextbox = document.querySelector('#new-name');

  if (!parentBookmark || !newNameTextbox)
    return;

  var parentId = parentBookmark.bookmarkId;
  var newFolderName = newNameTextbox.value;

  if (parentId == 0 || !newFolderName)
    return;

  chrome.bookmarks.create({
    title: newFolderName,
    parentId: parentId.toString()
  }, function(newNode) {
    for (var i in allTabs) {
      var tab = allTabs[i];
      chrome.bookmarks.create({'parentId': newNode.id.toString(),
                               'title': tab.title,
                               'url': tab.url});
    }
    window.close();
  });
}

/**
 * Adds handlers to buttons.
 */
document.querySelector('#new-folder').addEventListener('click', newFolder);
document.querySelector('#save').addEventListener('click', bookmarkAllTabs);
document.querySelector('#cancel').addEventListener('click', cancel);
