// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a simple pure JS event class that can be used with
 * {@code cr.ui.EventTarget}. It should not be used with DOM EventTargets.
 */

cr.define('cr', function() {

  // cr.Event is called CustomEvent in here to prevent naming conflicts. We
  // alse store the original Event in case someone does a global alias of
  // cr.Event.

  const DomEvent = Event;

  /**
   * Creates a new event to be used with cr.EventTarget or DOM EventTarget
   * objects.
   * @param {string} type The name of the event.
   * @param {boolean=}
   * @constructor
   */
  function CustomEvent(type, opt_bubbles, opt_capture) {
    var e = cr.doc.createEvent('Event');
    e.initEvent(type, !!opt_bubbles, !!opt_capture);
    e.__proto__ = CustomEvent.prototype;
    return e;
  }

  CustomEvent.prototype = {
    __proto__: DomEvent.prototype
  };

  // Export
  return {
    Event: CustomEvent
  };
});
