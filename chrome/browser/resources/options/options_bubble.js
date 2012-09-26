// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var BubbleBase = cr.ui.BubbleBase;

  var OptionsBubble = cr.ui.define('div');

  OptionsBubble.prototype = {
    // Set up the prototype chain.
    __proto__: BubbleBase.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      BubbleBase.prototype.decorate.call(this);
      this.classList.add('options-bubble');
    },

    /**
     * Show the bubble.
     */
    show: function() {
      if (!this.hidden)
        return;

      BubbleBase.prototype.show.call(this);

      var doc = this.ownerDocument;
      this.eventTracker_.add(doc, 'mousewheel', this, true);
      this.eventTracker_.add(doc, 'scroll', this, true);
      this.eventTracker_.add(doc, 'elementFocused', this, true);
      this.eventTracker_.add(window, 'resize', this);
    },

    /**
     * Handle events, closing the bubble when the user clicks or moves the focus
     * outside the bubble and its anchor elements, scrolls the underlying
     * document or resizes the window.
     * @param {Event} event The event.
     */
    handleEvent: function(event) {
      BubbleBase.prototype.handleEvent.call(this, event);

      switch (event.type) {
        // Close the bubble when the user clicks outside it, except if it is a
        // left-click on the anchor element (allowing the anchor to handle the
        // event and close the bubble itself).
        case 'mousedown':
          if (event.button == 0 && this.anchorNode_.contains(event.target))
            break;
        // Close the bubble when the underlying document is scrolled.
        case 'mousewheel':
        case 'scroll':
          if (this.contains(event.target))
            break;
        // Close the bubble when the window is resized.
        case 'resize':
          this.hide();
          break;
        // Close the bubble when the focus moves to an element that is not the
        // anchor and is not inside the bubble.
        case 'elementFocused':
          if (!this.anchorNode_.contains(event.target) &&
              !this.contains(event.target)) {
            this.hide();
          }
          break;
      }
    },

    /**
     * Attach the bubble to the document's DOM, making it a sibling of the
     * anchor element so that focusable elements inside the bubble follow the
     * anchor in the document's tab order.
     * @private
     */
    attachToDOM_: function() {
      var parent = this.anchorNode_.parentNode;
      parent.insertBefore(this, this.anchorNode_.nextSibling);
    },
  };

  return {
    OptionsBubble: OptionsBubble
  };
});
