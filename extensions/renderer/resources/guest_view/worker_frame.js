// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the WorkerFrame prototype.

var GuestView = require('guestView').GuestView;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;

function WorkerFrameImpl(workerFrameElement) {
  GuestViewContainer.call(this, workerFrameElement, 'workerframe');
}

WorkerFrameImpl.prototype.__proto__ = GuestViewContainer.prototype;

WorkerFrameImpl.VIEW_TYPE = 'WorkerFrame';

// Add extra functionality to |this.element|.
WorkerFrameImpl.setupElement = function(proto) {
  var apiMethods = [
    'connect'
  ];

  // Forward proto.foo* method calls to WorkerFrameImpl.foo*.
  GuestViewContainer.forwardApiMethods(proto, apiMethods);
}

WorkerFrameImpl.prototype.connect = function(url, callback) {
  if (!this.elementAttached) {
    if (callback) {
      callback(false);
    }
    return;
  }

  this.guest.destroy();

  var createParams = {
    'url': url
  };

  this.guest.create(createParams, function() {
    this.attachWindow();
    if (callback) {
      callback(true);
    }
  }.bind(this));
};

GuestViewContainer.registerElement(WorkerFrameImpl);
