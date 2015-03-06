// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the SurfaceView prototype.

var GuestView = require('guestView').GuestView;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;

function SurfaceViewImpl(surfaceviewElement) {
  GuestViewContainer.call(this, surfaceviewElement, 'surfaceview');
}

SurfaceViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

SurfaceViewImpl.VIEW_TYPE = 'SurfaceView';

// Add extra functionality to |this.element|.
SurfaceViewImpl.setupElement = function(proto) {
  var apiMethods = [
    'connect'
  ];

  // Forward proto.foo* method calls to SurfaceViewImpl.foo*.
  GuestViewContainer.forwardApiMethods(proto, apiMethods);
}

SurfaceViewImpl.prototype.buildContainerParams = function() {
  return { 'url': this.url };
};

SurfaceViewImpl.prototype.connect = function(url, callback) {
  if (!this.elementAttached) {
    if (callback) {
      callback(false);
    }
    return;
  }

  this.url = url;

  this.guest.destroy();

  this.guest.create(this.buildParams(), function() {
    this.attachWindow();
    if (callback) {
      callback(true);
    }
  }.bind(this));
};

GuestViewContainer.registerElement(SurfaceViewImpl);
