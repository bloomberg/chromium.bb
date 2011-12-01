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
cr.define('cr.ui', function() {
  'use strict';

  /**
   * @constructor
   * @param {!Element} frame The bounding rectangle that cards are visible in.
   * @param {!Element} container The surrounding element that will have event
   *     listeners attached to it.
   * @param {number} cardWidth The width of each card should have.
   */
  function CardSlider(frame, container, cardWidth) {
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
     * Array of card elements.
     * @type {!Array.<!Element>}
     * @private
     */
    this.cards_ = [];

    /**
     * Index of currently shown card.
     * @type {number}
     * @private
     */
    this.currentCard_ = 0;

    /**
     * @type {number}
     * @private
     */
    this.cardWidth_ = cardWidth;

    /**
     * @type {!cr.ui.TouchHandler}
     * @private
     */
    this.touchHandler_ = new cr.ui.TouchHandler(this.container_);
  }

  /**
   * Events fired by the slider.
   * Events are fired at the container.
   */
  CardSlider.EventType = {
    // Fired when the user slides to another card.
    CARD_CHANGED: 'cardSlider:card_changed'
  };


  /**
   * The time to transition between cards when animating. Measured in ms.
   * @type {number}
   * @private
   * @const
   */
  CardSlider.TRANSITION_TIME_ = 200;


  /**
   * The minimum velocity required to transition cards if they did not drag past
   * the halfway point between cards. Measured in pixels / ms.
   * @type {number}
   * @private
   * @const
   */
  CardSlider.TRANSITION_VELOCITY_THRESHOLD_ = 0.2;


  CardSlider.prototype = {
    /**
     * The current left offset of the container relative to the frame.
     * @type {number}
     * @private
     */
    currentLeft_: 0,

    /**
     * Initialize all elements and event handlers. Must call after construction
     * and before usage.
     */
    initialize: function() {
      var view = this.container_.ownerDocument.defaultView;
      assert(view.getComputedStyle(this.container_).display == '-webkit-box',
          'Container should be display -webkit-box.');
      assert(view.getComputedStyle(this.frame_).overflow == 'hidden',
          'Frame should be overflow hidden.');
      assert(view.getComputedStyle(this.container_).position == 'static',
          'Container should be position static.');

      this.updateCardWidths_();

      this.mouseWheelScrollAmount_ = 0;
      this.mouseWheelCardSelected_ = false;
      this.mouseWheelIsContinuous_ = false;
      this.scrollClearTimeout_ = null;
      this.frame_.addEventListener('mousewheel',
                                   this.onMouseWheel_.bind(this));

      // Also support touch events in case a touch screen happens to be
      // available.  Note that we could check cr.isTouchOptimized here, but
      // in general sites should just be listening for touch events without
      // trying to determine in advance if they may be supported (eg. a touch
      // screen can always be plugged in after a page has loaded).
      var TouchHandler = cr.ui.TouchHandler;
      this.container_.addEventListener(TouchHandler.EventType.TOUCH_START,
                                       this.onTouchStart_.bind(this));
      this.container_.addEventListener(TouchHandler.EventType.DRAG_START,
                                       this.onDragStart_.bind(this));
      this.container_.addEventListener(TouchHandler.EventType.DRAG_MOVE,
                                       this.onDragMove_.bind(this));
      this.container_.addEventListener(TouchHandler.EventType.DRAG_END,
                                       this.onDragEnd_.bind(this));

      this.touchHandler_.enable(/* opt_capture */ false);
    },

    /**
     * Use in cases where the width of the frame has changed in order to update
     * the width of cards. For example should be used when orientation changes
     * in full width sliders.
     * @param {number} newCardWidth Width all cards should have, in pixels.
     */
    resize: function(newCardWidth) {
      if (newCardWidth != this.cardWidth_) {
        this.cardWidth_ = newCardWidth;

        this.updateCardWidths_();

        // Must upate the transform on the container to show the correct card.
        this.transformToCurrentCard_();
      }
    },

    /**
     * Sets the cards used. Can be called more than once to switch card sets.
     * @param {!Array.<!Element>} cards The individual viewable cards.
     * @param {number} index Index of the card to in the new set of cards to
     *     navigate to.
     */
    setCards: function(cards, index) {
      assert(index >= 0 && index < cards.length,
          'Invalid index in CardSlider#setCards');
      this.cards_ = cards;

      this.updateCardWidths_();

      // Jump to the given card index.
      this.selectCard(index);
    },

    /**
     * Updates the width of each card.
     * @private
     */
    updateCardWidths_: function() {
      for (var i = 0, card; card = this.cards_[i]; i++)
        card.style.width = this.cardWidth_ + 'px';
    },

    /**
     * Returns the index of the current card.
     * @return {number} index of the current card.
     */
    get currentCard() {
      return this.currentCard_;
    },

    /**
     * Allows setting the current card index.
     * @param {number} index A new index to set the current index to.
     * @return {number} The new index after having been set.
     */
    set currentCard(index) {
      return (this.currentCard_ = index);
    },

    /**
     * Returns the number of cards.
     * @return {number} number of cards.
     */
    get cardCount() {
      return this.cards_.length;
    },

    /**
     * Returns the current card itself.
     * @return {!Element} the currently shown card.
     */
    get currentCardValue() {
      return this.cards_[this.currentCard_];
    },

    /**
     * Handle horizontal scrolls to flip between pages.
     * @private
     */
    onMouseWheel_: function(e) {
      if (e.wheelDeltaX == 0)
        return;

      // Prevent OS X 10.7+ history swiping on the NTP.
      e.preventDefault();

      // Continuous devices such as an Apple Touchpad or Apple MagicMouse will
      // send arbitrary delta values. Conversly, standard mousewheels will
      // send delta values in increments of 120.  (There is of course a small
      // chance we mistake a continuous device for a non-continuous device.
      // Unfortunately there isn't a better way to do this until real touch
      // events are available to desktop clients.)
      var DISCRETE_DELTA = 120;
      if (e.wheelDeltaX % DISCRETE_DELTA)
        this.mouseWheelIsContinuous_ = true;

      if (this.mouseWheelIsContinuous_) {
        // For continuous devices, detect a page swipe when the accumulated
        // delta matches a pre-defined threshhold.  After changing the page,
        // ignore wheel events for a short time before repeating this process.
        if (this.mouseWheelCardSelected_) return;
        this.mouseWheelScrollAmount_ += e.wheelDeltaX;
        if (Math.abs(this.mouseWheelScrollAmount_) >= 600) {
          var pagesToScroll = this.mouseWheelScrollAmount_ > 0 ? 1 : -1;
          if (!isRTL())
            pagesToScroll *= -1;
          var newCardIndex = this.currentCard + pagesToScroll;
          newCardIndex = Math.min(this.cards_.length - 1,
                                  Math.max(0, newCardIndex));
          this.selectCard(newCardIndex, true);
          this.mouseWheelCardSelected_ = true;
        }
      } else {
        // For discrete devices, consider each wheel tick a page change.
        var pagesToScroll = e.wheelDeltaX / DISCRETE_DELTA;
        if (!isRTL())
          pagesToScroll *= -1;
        var newCardIndex = this.currentCard + pagesToScroll;
        newCardIndex = Math.min(this.cards_.length - 1,
                                Math.max(0, newCardIndex));
        this.selectCard(newCardIndex, true);
      }

      // We got a mouse wheel event, so cancel any pending scroll wheel timeout.
      if (this.scrollClearTimeout_ != null)
        clearTimeout(this.scrollClearTimeout_);
      // If we didn't use up all the scroll, hold onto it for a little bit, but
      // drop it after a delay.
      if (this.mouseWheelScrollAmount_ != 0) {
        this.scrollClearTimeout_ =
            setTimeout(this.clearMouseWheelScroll_.bind(this), 500);
      }
    },

    /**
     * Resets the amount of horizontal scroll we've seen to 0. See
     * onMouseWheel_.
     * @private
     */
    clearMouseWheelScroll_: function() {
      this.mouseWheelScrollAmount_ = 0;
      this.mouseWheelCardSelected_ = false;
    },

    /**
     * Selects a new card, ensuring that it is a valid index, transforming the
     * view and possibly calling the change card callback.
     * @param {number} newCardIndex Index of card to show.
     * @param {boolean=} opt_animate If true will animate transition from
     *     current position to new position.
     */
    selectCard: function(newCardIndex, opt_animate) {
      var previousCard = this.currentCardValue;

      var isChangingCard =
          !this.cards_[newCardIndex].classList.contains('selected-card');

      if (isChangingCard) {
        previousCard.classList.remove('selected-card');
        // If we have a new card index and it is valid then update the left
        // position and current card index.
        this.currentCard_ = newCardIndex;
        this.currentCardValue.classList.add('selected-card');
      }

      this.transformToCurrentCard_(opt_animate);

      if (isChangingCard) {
        var event = document.createEvent('Event');
        event.initEvent(CardSlider.EventType.CARD_CHANGED, true, true);
        event.cardSlider = this;
        this.container_.dispatchEvent(event);

        // We also dispatch an event on the cards themselves.
        if (previousCard) {
          cr.dispatchSimpleEvent(previousCard, 'carddeselected',
                                 true, true);
        }
        cr.dispatchSimpleEvent(this.currentCardValue, 'cardselected',
                               true, true);
      }
    },

    /**
     * Selects a card from the stack. Passes through to selectCard.
     * @param {Node} newCard The card that should be selected.
     * @param {boolean=} opt_animate Whether to animate.
     */
    selectCardByValue: function(newCard, opt_animate) {
      var i = this.cards_.indexOf(newCard);
      assert(i != -1);
      this.selectCard(i, opt_animate);
    },

    /**
     * Centers the view on the card denoted by this.currentCard. Can either
     * animate to that card or snap to it.
     * @param {boolean=} opt_animate If true will animate transition from
     *     current position to new position.
     * @private
     */
    transformToCurrentCard_: function(opt_animate) {
      this.currentLeft_ = -this.cardWidth_ *
          (isRTL() ? this.cards_.length - this.currentCard - 1 :
                     this.currentCard);

      // Animate to the current card, which will either transition if the
      // current card is new, or reset the existing card if we didn't drag
      // enough to change cards.
      var transition = '';
      if (opt_animate) {
        transition = '-webkit-transform ' + CardSlider.TRANSITION_TIME_ +
                     'ms ease-in-out';
      }
      this.container_.style.WebkitTransition = transition;
      this.translateTo_(this.currentLeft_);
    },

    /**
     * Moves the view to the specified position.
     * @param {number} x Horizontal position to move to.
     * @private
     */
    translateTo_: function(x) {
      // We use a webkitTransform to slide because this is GPU accelerated on
      // Chrome and iOS.  Once Chrome does GPU acceleration on the position
      // fixed-layout elements we could simply set the element's position to
      // fixed and modify 'left' instead.
      this.container_.style.WebkitTransform = 'translate3d(' + x + 'px, 0, 0)';
    },

    /* Touch ******************************************************************/

    /**
     * Clear any transition that is in progress and enable dragging for the
     * touch.
     * @param {!cr.ui.TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onTouchStart_: function(e) {
      this.container_.style.WebkitTransition = '';
      e.enableDrag = true;
    },

    /**
     * Tell the TouchHandler that dragging is acceptable when the user begins by
     * scrolling horizontally.
     * @param {!cr.ui.TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onDragStart_: function(e) {
      e.enableDrag = Math.abs(e.dragDeltaX) > Math.abs(e.dragDeltaY);
    },

    /**
     * On each drag move event reposition the container appropriately so the
     * cards look like they are sliding.
     * @param {!cr.ui.TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onDragMove_: function(e) {
      var deltaX = e.dragDeltaX;
      // If dragging beyond the first or last card then apply a backoff so the
      // dragging feels stickier than usual.
      if (!this.currentCard && deltaX > 0 ||
          this.currentCard == (this.cards_.length - 1) && deltaX < 0) {
        deltaX /= 2;
      }
      this.translateTo_(this.currentLeft_ + deltaX);
    },

    /**
     * On drag end events we may want to transition to another card, depending
     * on the ending position of the drag and the velocity of the drag.
     * @param {!cr.ui.TouchHandler.Event} e The TouchHandler event.
     * @private
     */
    onDragEnd_: function(e) {
      var deltaX = e.dragDeltaX;
      var velocity = this.touchHandler_.getEndVelocity().x;
      var newX = this.currentLeft_ + deltaX;
      var newCardIndex = Math.round(-newX / this.cardWidth_);

      if (newCardIndex == this.currentCard && Math.abs(velocity) >
          CardSlider.TRANSITION_VELOCITY_THRESHOLD_) {
        // If the drag wasn't far enough to change cards but the velocity was
        // high enough to transition anyways. If the velocity is to the left
        // (negative) then the user wishes to go right (card +1).
        newCardIndex += velocity > 0 ? -1 : 1;
      }

      this.selectCard(newCardIndex, /* animate */ true);
    },

    /**
     * Cancel any current touch/slide as if we saw a touch end
     */
    cancelTouch: function() {
      // Stop listening to any current touch
      this.touchHandler_.cancelTouch();

      // Ensure we're at a card bounary
      this.transformToCurrentCard_(true);
    },
  };

  return {
    CardSlider: CardSlider
  };
});
