// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Card slider implementation. Allows you to create interactions
 * that have items that can slide left to right to reveal additional items.
 * Works by adding the necessary event handlers to a specific DOM structure
 * including a frame, container and cards.
 * - The frame defines the boundary of one item. Each card will be expanded to
 *   fill the width of the frame. This element is also overflow hidden so that
 *   the additional items left / right do not trigger horizontal scrolling.
 * - The container is what all the touch events are attached to. This element
 *   will be expanded to be the width of all cards.
 * - The cards are the individual viewable items. There should be one card for
 *   each item in the list. Only one card will be visible at a time. Two cards
 *   will be visible while you are transitioning between cards.
 *
 * This class is designed to work well on any hardware-accelerated touch device.
 * It should still work on pre-hardware accelerated devices it just won't feel
 * very good. It should also work well with a mouse.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome
var Slider = (function() {
  'use strict';

  /**
   * The type of Event sent by Slider
   * @constructor
   * @param {string} type The type of event (one of Slider.EventType).
   * @param {Element!} targetCard The element being dragged.
   */
  Slider.Event = function(type, activeCardIndex) {
    var event = document.createEvent('Event');
    event.initEvent(type, true, true);
    event.__proto__ = Slider.Event.prototype;

    event.activeCardIndex = activeCardIndex;

    return event;
  };

  Slider.Event.prototype = {
    __proto__: Event.prototype
  };

  /**
   * @constructor
   * @param {!Element} frame The bounding rectangle that cards are visible in.
   * @param {!Element} container The surrounding element that will have event
   *     listeners attached to it.
   * @param {!Array.<!Element>} cards The individual viewable cards.
   */
  function Slider(frame, container, cards) {
    /**
     * @type {!Element}
     * @private
     */
    this.frame_ = frame;

    /**
     * @type {!Element}
     * @private
     */
    this.container_ = container;

    /**
     * @type {!Array.<!Element>}
     * @private
     */
    this.cards_ = cards;

    this.lastLeft_ = 0;

    this.ignoreClicks_ = false;

    /**
     * @type {!TouchHandler}
     * @private
     */
    this.touchHandler_ = new TouchHandler(this.container_);
  }

  /**
   * Events fired by the slider.
   * Events are fired at the container.
   */
  Slider.EventType = {
    // Fired when the user slides to another card.
    SELECTION_CHANGED: 'slider:selection_changed',
    // Fired at the card element when it is activated.
    ACTIVATE: 'slider:activate',
    // Fired at the card elment when it is deactivated.
    DEACTIVATE: 'slider:deactivate',
  };

  /**
   * The time to transition between cards when animating. Measured in ms.
   * @type {number}
   * @private
   * @const
   */
  Slider.TRANSITION_TIME_ = 200;

  /**
   * The minimum velocity required to transition cards if they did not drag past
   * the halfway point between cards. Measured in pixels / ms.
   * @type {number}
   * @private
   * @const
   */
  Slider.TRANSITION_VELOCITY_THRESHOLD_ = 0.2;

  Slider.prototype = {
    currentCard_: null,

    /**
     * Initialize all elements and event handlers. Must call after construction
     * and before usage.
     */
    initialize: function() {
      var view = this.container_.ownerDocument.defaultView;
      assert(view.getComputedStyle(this.frame_).overflow == 'hidden',
          'Frame should be overflow hidden.');
      assert(view.getComputedStyle(this.container_).position == 'static',
          'Container should be position static.');
      for (var i = 0, card; card = this.cards_[i]; i++) {
        assert(view.getComputedStyle(card).position == 'static',
            'Cards should be position static.');
      }

      var self = this;
      var clickSelf = function(e) {
        self.onClick_(self, e);
      };
      this.container_.addEventListener('click', clickSelf , true);
      this.touchHandler_.enable(/* opt_capture */ false);
    },

    /**
     * Sets the cards used. Can be called more than once to switch card sets.
     * @param {!Array.<!Element>} cards The individual viewable cards.
     * @param {number} index Index of the card to in the new set of cards to
     *     navigate to.
     */
    setCards: function(cards, index) {
      assert(index >= 0 && index < cards.length,
          'Invalid index in Slider#setCards');
      this.cards_ = cards;
    },

    cardWidth: function() {
      return this.cards_.length == 0 ? 0 : this.cards_[0].offsetWidth;
    },

    cardCount: function() {
      return this.cards_.length;
    },

    get currentCard() {
      if (this.currentCardIndex == null) {
        return null;
      }
      return this.cards_[this.currentCard_];
    },

    /**
     * Returns the index of the current card.
     * @return {number} index of the current card.
     */
    get currentCardIndex() {
      return this.currentCard_;
    },

    set currentCardIndex(card) {
      // There's nothing to do if nothing has changed.
      if (card == this.currentCard_) {
        return;
      }
      if (this.currentCard_ != null) {
        this.sendEvent_(Slider.EventType.DEACTIVATE,
                        this.cards_[this.currentCard_], null);
      }
      var oldCard = this.currentCard_;
      this.currentCard_ = card;
      // If no cards are selected, restored the last scroll position,
      // and re-enable the cards.
      if (this.currentCard_ == null) {
        this.sendEvent_(Slider.EventType.SELECTION_CHANGED, this.frame_, null);
      } else {
        // Center the newly selected card.
        var cardPosition = card * this.cardWidth();
        var centerOfContainer =
            (this.frame_.offsetWidth - this.cardWidth()) / 2;
      }
      if (this.currentCard_ != null) {
        this.sendEvent_(Slider.EventType.SELECTION_CHANGED, this.frame_,
                        this.currentCard_);
        // TODO(fsamuel): I can't think of a better way to do this currently.
        // We wait a little bit of time to finish the animation and then call
        // onActivateCard to make sure the WebUI doesn't resize while we're
        // animating.
        var self = this;
        setTimeout(function() {
          self.sendEvent_(Slider.EventType.ACTIVATE,
                          self.cards_[self.currentCard_],
                          self.currentCard_);
        }, 80);
      } // if
    },

    get ignoreClicks() {
      return this.ignoreClicks_;
    },

    onClick_: function(self, e) {
      if (self.ignoreClicks) {
        self.ignoreClicks_ = false;
        e.stopPropagation();
      }
    },

    /**
     * Cancel any current touch/slide as if we saw a touch end
     */
    cancelTouch: function() {
      // Stop listening to any current touch
      this.touchHandler_.cancelTouch();
      this.ignoreClicks_ = false;
    },

    /**
     * Send a Slider event to a specific card element
     * @param {string} eventType The type of event to send.
     * @param {!Element} target The element to send the event to.
     * @private
     */
    sendEvent_: function(eventType, target, activeCardIndex) {
      var event = new Slider.Event(eventType, activeCardIndex);
      target.dispatchEvent(event);
    },
  };

  return Slider;
})();
