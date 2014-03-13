// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');

/**
 * A single node in the Automation tree.
 * @param {AutomationTree} owner The owning tree.
 * @constructor
 */
var AutomationNodeImpl = function(owner) {
  this.owner = owner;
  this.child_ids = [];

  this.attributes = {};
};

AutomationNodeImpl.prototype = {
  parent: function() {
    return this.owner.get(this.parent_id);
  },

  firstChild: function() {
    var node = this.owner.get(this.child_ids[0]);
    return node;
  },

  lastChild: function() {
    var child_ids = this.child_ids;
    var node = this.owner.get(child_ids[child_ids.length - 1]);
    return node;
  },

  children: function() {
    var children = [];
    for (var i = 0, child_id; child_id = this.child_ids[i]; i++)
      children.push(this.owner.get(child_id));
    return children;
  },

  previousSibling: function() {
    var parent = this.parent();
    if (parent && this.index_in_parent > 0)
      return parent.children()[this.index_in_parent - 1];
    return undefined;
  },

  nextSibling: function() {
    var parent = this.parent();
    if (parent && this.index_in_parent < parent.children().length)
      return parent.children()[this.index_in_parent + 1];
    return undefined;
  },
};


var AutomationNode = utils.expose('AutomationNode',
                                  AutomationNodeImpl,
                                  ['parent',
                                   'firstChild',
                                   'lastChild',
                                   'children',
                                   'previousSibling',
                                   'nextSibling']);
exports.AutomationNode = AutomationNode;
