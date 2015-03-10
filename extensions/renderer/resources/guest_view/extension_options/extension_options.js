// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ExtensionOptionsEvents =
    require('extensionOptionsEvents').ExtensionOptionsEvents;
var GuestView = require('guestView').GuestView;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;

function ExtensionOptionsImpl(extensionoptionsElement) {
  GuestViewContainer.call(this, extensionoptionsElement, 'extensionoptions');

  new ExtensionOptionsEvents(this);
  this.setupElementProperties();
};

ExtensionOptionsImpl.prototype.__proto__ = GuestViewContainer.prototype;

ExtensionOptionsImpl.VIEW_TYPE = 'ExtensionOptions';

ExtensionOptionsImpl.prototype.onElementAttached = function() {
  this.createGuest();
}

ExtensionOptionsImpl.prototype.buildContainerParams = function() {
  return {
    'extensionId': this.element.getAttribute('extension')
  };
};

ExtensionOptionsImpl.prototype.createGuest = function() {
  if (!this.elementAttached) {
    return;
  }

  // Destroy the old guest if one exists.
  this.guest.destroy();

  this.guest.create(this.buildParams(), function() {
    if (!this.guest.getId()) {
      // Fire a createfailed event here rather than in ExtensionOptionsGuest
      // because the guest will not be created, and cannot fire an event.
      var createFailedEvent = new Event('createfailed', { bubbles: true });
      this.dispatchEvent(createFailedEvent);
    } else {
      this.attachWindow();
    }
  }.bind(this));
};

ExtensionOptionsImpl.prototype.handleAttributeMutation =
    function(name, oldValue, newValue) {
  // We treat null attribute (attribute removed) and the empty string as
  // one case.
  oldValue = oldValue || '';
  newValue = newValue || '';

  if (oldValue === newValue)
    return;

  if (name == 'extension') {
    this.createGuest();
  }
};

ExtensionOptionsImpl.prototype.setupElementProperties = function() {
  $Object.defineProperty(this.element, 'extension', {
    get: function() {
      return this.element.getAttribute('extension');
    }.bind(this),
    set: function(value) {
      this.element.setAttribute('extension', value);
    }.bind(this),
    enumerable: true
  });
};

GuestViewContainer.registerElement(ExtensionOptionsImpl);
