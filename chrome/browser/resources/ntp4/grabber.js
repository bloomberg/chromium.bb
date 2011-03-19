// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Grabber implementation.
 * Allows you to pick up objects (with a long-press) and drag them around the
 * screen.
 *
 * Note: This should perhaps really use standard drag-and-drop events, but there
 * is no standard for them on touch devices.  We could define a model for
 * activating touch-based dragging of elements (programatically and/or with
 * CSS attributes) and use it here (even have a JS library to generate such
 * events when the browser doesn't support them).
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome)
var Grabber = (function() {
  'use strict';

  /**
   * Create a Grabber object to enable grabbing and dragging a given element.
   * @constructor
   * @param {!Element} element The element that can be grabbed and moved.
   */
  function Grabber(element) {
    /**
     * The element the grabber is attached to.
     * @type {!Element}
     * @private
     */
    this.element_ = element;

    /**
     * The TouchHandler responsible for firing lower-level touch events when the
     * element is manipulated.
     * @type {!TouchHandler}
     * @private
     */
    this.touchHandler_ = new TouchHandler(this.element);

    /**
     * Tracks all event listeners we have created.
     * @type {EventTracker}
     * @private
     */
    this.events_ = new EventTracker();

    // Enable the generation of events when the element is touched (but no need
    // to use the early capture phase of event processing).
    this.touchHandler_.enable(/* opt_capture */ false);

    // Prevent any built-in drag-and-drop support from activating for the
    // element. Note that we don't want details of how we're implementing
    // dragging here to leak out of this file (eg. we may switch to using webkit
    // drag-and-drop).
    this.events_.add(this.element, 'dragstart', function(e) {
      e.preventDefault();
    }, true);

    // Add our TouchHandler event listeners
    this.events_.add(this.element, TouchHandler.EventType.TOUCH_START,
        this.onTouchStart_.bind(this), false);
    this.events_.add(this.element, TouchHandler.EventType.LONG_PRESS,
        this.onLongPress_.bind(this), false);
    this.events_.add(this.element, TouchHandler.EventType.DRAG_START,
        this.onDragStart_.bind(this), false);
    this.events_.add(this.element, TouchHandler.EventType.DRAG_MOVE,
        this.onDragMove_.bind(this), false);
    this.events_.add(this.element, TouchHandler.EventType.DRAG_END,
        this.onDragEnd_.bind(this), false);
    this.events_.add(this.element, TouchHandler.EventType.TOUCH_END,
        this.onTouchEnd_.bind(this), false);
  }

  /**
   * Events fired by the grabber.
   * Events are fired at the element affected (not the element being dragged).
   * @enum {string}
   */
  Grabber.EventType = {
    // Fired at the grabber element when it is first grabbed
    GRAB: 'grabber:grab',
    // Fired at the grabber element when dragging begins (after GRAB)
    DRAG_START: 'grabber:dragstart',
    // Fired at an element when something is dragged over top of it.
    DRAG_ENTER: 'grabber:dragenter',
    // Fired at an element when something is no longer over top of it.
    // Not fired at all in the case of a DROP
    DRAG_LEAVE: 'grabber:drag',
    // Fired at an element when something is dropped on top of it.
    DROP: 'grabber:drop',
    // Fired at the grabber element when dragging ends (successfully or not) -
    // after any DROP or DRAG_LEAVE
    DRAG_END: 'grabber:dragend',
    // Fired at the grabber element when it is released (even if no drag
    // occured) - after any DRAG_END event.
    RELEASE: 'grabber:release'
  };

  /**
   * The type of Event sent by Grabber
   * @constructor
   * @param {string} type The type of event (one of Grabber.EventType).
   * @param {Element!} grabbedElement The element being dragged.
   */
  Grabber.Event = function(type, grabbedElement) {
    var event = document.createEvent('Event');
    event.initEvent(type, true, true);
    event.__proto__ = Grabber.Event.prototype;

    /**
     * The element which is being dragged.  For some events this will be the
     * same as 'target', but for events like DROP that are fired at another
     * element it will be different.
     * @type {!Element}
     */
    event.grabbedElement = grabbedElement;

    return event;
  };

  Grabber.Event.prototype = {
    __proto__: Event.prototype
  };


  /**
   * The CSS class to apply when an element is touched but not yet
   * grabbed.
   * @type {string}
   */
  Grabber.PRESSED_CLASS = 'grabber-pressed';

  /**
   * The class to apply when an element has been held (including when it is
   * being dragged.
   * @type {string}
   */
  Grabber.GRAB_CLASS = 'grabber-grabbed';

  /**
   * The class to apply when a grabbed element is being dragged.
   * @type {string}
   */
  Grabber.DRAGGING_CLASS = 'grabber-dragging';

  Grabber.prototype = {
    /**
     * @return {!Element} The element that can be grabbed.
     */
    get element() {
      return this.element_;
    },

    /**
     * Clean up all event handlers (eg. if the underlying element will be
     * removed)
     */
    dispose: function() {
      this.touchHandler_.disable();
      this.events_.removeAll();

      // Clean-up any active touch/drag
      if (this.dragging_)
        this.stopDragging_();
      this.onTouchEnd_();
    },

    /**
     * Invoked whenever this element is first touched
     * @param {!TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onTouchStart_: function(e) {
      this.element.classList.add(Grabber.PRESSED_CLASS);

      // Always permit the touch to perhaps trigger a drag
      e.enableDrag = true;
    },

    /**
     * Invoked whenever the element stops being touched.
     * Can be called explicitly to cleanup any active touch.
     * @param {!TouchHandler.Event=} opt_e The TouchHandler event.
     * @private
     */
    onTouchEnd_: function(opt_e) {
      if (this.grabbed_) {
        // Mark this element as no longer being grabbed
        this.element.classList.remove(Grabber.GRAB_CLASS);
        this.element.style.pointerEvents = '';
        this.grabbed_ = false;

        this.sendEvent_(Grabber.EventType.RELEASE, this.element);
      } else {
        this.element.classList.remove(Grabber.PRESSED_CLASS);
      }
    },

    /**
     * Handler for TouchHandler's LONG_PRESS event
     * Invoked when the element is held (without being dragged)
     * @param {!TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onLongPress_: function(e) {
      assert(!this.grabbed_, 'Got longPress while still being held');

      this.element.classList.remove(Grabber.PRESSED_CLASS);
      this.element.classList.add(Grabber.GRAB_CLASS);

      // Disable mouse events from the element - we care only about what's
      // under the element after it's grabbed (since we're getting move events
      // from the body - not the element itself).  Note that we can't wait until
      // onDragStart to do this because it won't have taken effect by the first
      // onDragMove.
      this.element.style.pointerEvents = 'none';

      this.grabbed_ = true;

      this.sendEvent_(Grabber.EventType.GRAB, this.element);
    },

    /**
     * Invoked when the element is dragged.
     * @param {!TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onDragStart_: function(e) {
      assert(!this.lastEnter_, 'only expect one drag to occur at a time');
      assert(!this.dragging_);

      // We only want to drag the element if its been grabbed
      if (this.grabbed_) {
        // Mark the item as being dragged
        // Ensures our translate transform won't be animated and cancels any
        // outstanding animations.
        this.element.classList.add(Grabber.DRAGGING_CLASS);

        // Determine the webkitTransform currently applied to the element.
        // Note that it's important that we do this AFTER cancelling animation,
        // otherwise we could see an intermediate value.
        // We'll assume this value will be constant for the duration of the drag
        // so that we can combine it with our translate3d transform.
        this.baseTransform_ = this.element.ownerDocument.defaultView.
            getComputedStyle(this.element).webkitTransform;

        this.sendEvent_(Grabber.EventType.DRAG_START, this.element);
        e.enableDrag = true;
        this.dragging_ = true;

      } else {
        // Hasn't been grabbed - don't drag, just unpress
        this.element.classList.remove(Grabber.PRESSED_CLASS);
        e.enableDrag = false;
      }
    },

    /**
     * Invoked when a grabbed element is being dragged
     * @param {!TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onDragMove_: function(e) {
      assert(this.grabbed_ && this.dragging_);

      this.translateTo_(e.dragDeltaX, e.dragDeltaY);

      var target = e.touchedElement;
      if (target && target != this.lastEnter_) {
        // Send the events
        this.sendDragLeave_(e);
        this.sendEvent_(Grabber.EventType.DRAG_ENTER, target);
      }
      this.lastEnter_ = target;
    },

    /**
     * Send DRAG_LEAVE to the element last sent a DRAG_ENTER if any.
     * @param {!TouchHandler.Event} e The event triggering this DRAG_LEAVE.
     * @private
     */
    sendDragLeave_: function(e) {
      if (this.lastEnter_) {
        this.sendEvent_(Grabber.EventType.DRAG_LEAVE, this.lastEnter_);
        this.lastEnter_ = undefined;
      }
    },

    /**
     * Moves the element to the specified position.
     * @param {number} x Horizontal position to move to.
     * @param {number} y Vertical position to move to.
     * @private
     */
    translateTo_: function(x, y) {
      // Order is important here - we want to translate before doing the zoom
      this.element.style.WebkitTransform = 'translate3d(' + x + 'px, ' +
          y + 'px, 0) ' + this.baseTransform_;
    },

    /**
     * Invoked when the element is no longer being dragged.
     * @param {TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onDragEnd_: function(e) {
      // We should get this before the onTouchEnd.  Don't change
      // this.grabbed_ - it's onTouchEnd's responsibility to clear it.
      assert(this.grabbed_ && this.dragging_);
      var event;

      // Send the drop event to the element underneath the one we're dragging.
      var target = e.touchedElement;
      if (target)
        this.sendEvent_(Grabber.EventType.DROP, target);

      // Cleanup and send DRAG_END
      // Note that like HTML5 DND, we don't send DRAG_LEAVE on drop
      this.stopDragging_();
    },

    /**
     * Clean-up the active drag and send DRAG_LEAVE
     * @private
     */
    stopDragging_: function() {
      assert(this.dragging_);
      this.lastEnter_ = undefined;

      // Mark the element as no longer being dragged
      this.element.classList.remove(Grabber.DRAGGING_CLASS);
      this.element.style.webkitTransform = '';

      this.dragging_ = false;
      this.sendEvent_(Grabber.EventType.DRAG_END, this.element);
    },

    /**
     * Send a Grabber event to a specific element
     * @param {string} eventType The type of event to send.
     * @param {!Element} target The element to send the event to.
     * @private
     */
    sendEvent_: function(eventType, target) {
      var event = new Grabber.Event(eventType, this.element);
      target.dispatchEvent(event);
    },

    /**
     * Whether or not the element is currently grabbed.
     * @type {boolean}
     * @private
     */
    grabbed_: false,

    /**
     * Whether or not the element is currently being dragged.
     * @type {boolean}
     * @private
     */
    dragging_: false,

    /**
     * The webkitTransform applied to the element when it first started being
     * dragged.
     * @type {string|undefined}
     * @private
     */
    baseTransform_: undefined,

    /**
     * The element for which a DRAG_ENTER event was last fired
     * @type {Element|undefined}
     * @private
     */
    lastEnter_: undefined
  };

  return Grabber;
})();
