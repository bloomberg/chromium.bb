// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Page switcher
 * This is the class for the left and right navigation arrows that switch
 * between pages.
 */
cr.define('ntp', function() {

  function PageSwitcher() {
  }

  PageSwitcher.template = {
    __proto__: HTMLButtonElement.prototype,

    decorate: function(el) {
      el.__proto__ = PageSwitcher.template;

      el.addEventListener('click', el.activate_);

      el.direction_ = el.id == 'page-switcher-start' ? -1 : 1;

      el.dragWrapper_ = new cr.ui.DragWrapper(el, el);
    },

    /**
     * Activate the switcher (go to the next card).
     * @private
     */
    activate_: function() {
      var cardSlider = ntp.getCardSlider();
      var index = cardSlider.currentCard + this.direction_;
      var numCards = cardSlider.cardCount - 1;
      cardSlider.selectCard(Math.max(0, Math.min(index, numCards)), true);
    },

    shouldAcceptDrag: function(e) {
      // We allow all drags to trigger the page switching effect.
      return true;
    },

    doDragEnter: function(e) {
      this.scheduleDelayedSwitch_();
      this.doDragOver(e);
    },

    doDragLeave: function(e) {
      this.cancelDelayedSwitch_();
    },

    doDragOver: function(e) {
      e.preventDefault();
      var targetPage = ntp.getCardSlider().currentCardValue;
      if (targetPage.shouldAcceptDrag(e))
        targetPage.setDropEffect(e.dataTransfer);
    },

    doDrop: function(e) {
      e.stopPropagation();
      this.cancelDelayedSwitch_();

      var tile = ntp.getCurrentlyDraggingTile();
      if (!tile)
        return;

      var sourcePage = tile.tilePage;
      var targetPage = ntp.getCardSlider().currentCardValue;
      if (targetPage == sourcePage || !targetPage.shouldAcceptDrag(e))
        return;

      targetPage.appendDraggingTile();
    },

    /**
     * Starts a timer to activate the switcher. The timer repeats until
     * cancelled by cancelDelayedSwitch_.
     * @private
     */
    scheduleDelayedSwitch_: function() {
      var self = this;
      function navPageClearTimeout() {
        self.activate_();
        self.dragNavTimeout_ = null;
        self.scheduleDelayedSwitch_();
      }
      this.dragNavTimeout_ = window.setTimeout(navPageClearTimeout, 500);
    },

    /**
     * Cancels the timer that activates the switcher while dragging.
     * @private
     */
    cancelDelayedSwitch_: function() {
      if (this.dragNavTimeout_) {
        window.clearTimeout(this.dragNavTimeout_);
        this.dragNavTimeout_ = null;
      }
    },

  };

  return {
    initializePageSwitcher: PageSwitcher.template.decorate
  };
});
