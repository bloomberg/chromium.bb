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
var routingIdToAutomationTree = {};
var routingIdToCallback = {};

automation.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('getTree', function(callback) {
    // enableCurrentTab() ensures the renderer for the current tab has
    // accessibility enabled, and fetches its routing id to use as a key in the
    // routingIdToAutomationTree map. The callback to enableCurrentTab is bound
    // to the callback passed in to getTree(), so that once the tree is
    // available (either due to having been cached earlier, or after an
    // accessibility event occurs which causes the tree to be populated), the
    // callback can be called.
    automationInternal.enableCurrentTab(function(rid) {
      var targetTree = routingIdToAutomationTree[rid];
      if (!targetTree) {
        // If we haven't cached the tree, hold the callback until the tree is
        // populated by the initial onAccessibilityEvent call.
        if (rid in routingIdToCallback)
          routingIdToCallback[rid].push(callback);
        else
          routingIdToCallback[rid] = [callback];
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
  var rid = data.routing_id;
  var targetTree = routingIdToAutomationTree[rid];
  if (!targetTree) {
    // If this is the first time we've gotten data for this tree, it will
    // contain all of the tree's data, so create a new tree which will be
    // bootstrapped from |data|.
    targetTree = new AutomationTree(rid);
    routingIdToAutomationTree[rid] = targetTree;
  }
  if (privates(targetTree).impl.update(data)) {
    // TODO(aboxhall/dtseng): remove and replace with EventListener style API
    targetTree.onUpdate.dispatch();
  }

  // TODO(aboxhall/dtseng): call appropriate event listeners based on
  // data.event_type.

  // If the tree wasn't available when getTree() was called, the callback will
  // have been cached in routingIdToCallback, so call and delete it now that we
  // have the tree.
  if (rid in routingIdToCallback) {
    for (var i = 0; i < routingIdToCallback[rid].length; i++) {
      var callback = routingIdToCallback[rid][i];
      callback(targetTree);
    }
    delete routingIdToCallback[rid];
  }
});

exports.binding = automation.generate();
