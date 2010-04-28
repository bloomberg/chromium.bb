// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This provides a nicer way to create events than what DOM
 * provides. These events can be used with DOM EventTarget interfaces as well
 * as with {@code cr.EventTarget}.
 */

cr.define('cr', function() {

  // cr.Event is called CrEvent in here to prevent naming conflicts. We also
  // store the original Event in case someone does a global alias of cr.Event.

  const DomEvent = Event;

  /**
   * Creates a new event to be used with cr.EventTarget or DOM EventTarget
   * objects.
   * @param {string} type The name of the event.
   * @param {boolean=} opt_bubbles Whether the event bubbles. Default is false.
   * @param {boolean=} opt_preventable Whether the default action of the event
   *     can be prevented.
   * @constructor
   * @extends {DomEvent}
   */
  function CrEvent(type, opt_bubbles, opt_preventable) {
    var e = cr.doc.createEvent('Event');
    e.initEvent(type, !!opt_bubbles, !!opt_preventable);
    e.__proto__ = CrEvent.prototype;
    return e;
  }

  CrEvent.prototype = {
    __proto__: DomEvent.prototype
  };

  // Export
  return {
    Event: CrEvent
  };
});
