// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the automation API.
var automation = require('binding').Binding.create('automation');
var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var eventBindings = require('event_bindings');
var Event = eventBindings.Event;
var AutomationNode = require('automationNode').AutomationNode;
var AutomationTree = require('automationTree').AutomationTree;

// TODO(aboxhall): Look into using WeakMap
var idToAutomationTree = {};
var idToCallback = {};

// TODO(dtseng): Move out to automation/automation_util.js or as a static member
// of AutomationTree to keep this file clean.
/*
 * Creates an id associated with a particular AutomationTree based upon a
 * renderer/renderer host pair's process and routing id.
 */
var createAutomationTreeID = function(pid, rid) {
  return pid + '_' + rid;
};

automation.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('getTree', function(callback) {
    // enableCurrentTab() ensures the renderer for the current tab has
    // accessibility enabled, and fetches its process and routing ids to use as
    // a key in the idToAutomationTree map. The callback to enableCurrentTab is
    // bound to the callback passed in to getTree(), so that once the tree is
    // available (either due to having been cached earlier, or after an
    // accessibility event occurs which causes the tree to be populated), the
    // callback can be called.
    automationInternal.enableCurrentTab(function(pid, rid) {
      var id = createAutomationTreeID(pid, rid);
      var targetTree = idToAutomationTree[id];
      if (!targetTree) {
        // If we haven't cached the tree, hold the callback until the tree is
        // populated by the initial onAccessibilityEvent call.
        if (id in idToCallback)
          idToCallback[id].push(callback);
        else
          idToCallback[id] = [callback];
      } else {
        callback(targetTree);
      }
    });
  });
});

// Listen to the automationInternal.onaccessibilityEvent event, which is
// essentially a proxy for the AccessibilityHostMsg_Events IPC from the
// renderer.
automationInternal.onAccessibilityEvent.addListener(function(data) {
  var pid = data.processID;
  var rid = data.routingID;
  var id = createAutomationTreeID(pid, rid);
  var targetTree = idToAutomationTree[id];
  if (!targetTree) {
    // If this is the first time we've gotten data for this tree, it will
    // contain all of the tree's data, so create a new tree which will be
    // bootstrapped from |data|.
    targetTree = new AutomationTree(pid, rid);
    idToAutomationTree[id] = targetTree;
  }
  privates(targetTree).impl.update(data);

  // If the tree wasn't available when getTree() was called, the callback will
  // have been cached in idToCallback, so call and delete it now that we
  // have the tree.
  if (id in idToCallback) {
    for (var i = 0; i < idToCallback[id].length; i++) {
      var callback = idToCallback[id][i];
      callback(targetTree);
    }
    delete idToCallback[id];
  }
});

exports.binding = automation.generate();
