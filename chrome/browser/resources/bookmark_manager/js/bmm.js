// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('bmm', function() {
  function isFolder(bookmarkNode) {
    return !bookmarkNode.url;
  }

  function contains(parent, descendant) {
    if (descendant.parentId == parent.id)
      return true;
    // the bmm.treeLookup contains all folders
    var parentTreeItem = bmm.treeLookup[descendant.parentId];
    if (!parentTreeItem || !parentTreeItem.bookmarkNode)
      return false;
    return this.contains(parent, parentTreeItem.bookmarkNode);
  }

  return {
    isFolder: isFolder,
    contains: contains
  };
});
