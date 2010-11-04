// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A zippy is an area with a clickable header that controls whether the rest of
// the area is displayed or not. In most zippies the header contains a +/- sign
// to indicate the current state, and also to suggest that the content can be
// expanded (when collapsed) or collapsed (when expanded).

// This implementation smoothly animates the content of the zippy when it is
// expanded and collapsed, using the -webkit-transition style attribute.

cr.define('options', function() {

  //////////////////////////////////////////////////////////////////////////////
  // Zippy class:

  var Zippy = cr.ui.define('div');

  Zippy.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.classList.add('collapsed');
      this.classList.add('measure');
      var self = this;
      this.addEventListener('click', function(e) {
        var target = e.target;
        while (target && !target.classList.contains('zippy')) {
          if (target.classList.contains('zippy-header')) {
            self.toggle();
            break;
          }
          target = target.parentNode;
        }
      });
    },

    /**
     * Toggle the expanded state of this zippy.
     */
    toggle: function() {
      this.expanded = !this.expanded;
    },

    /**
     * Do an initial measure of this zippy.
     *
     * This function is called just after making the zippy's contents visible
     * for the first time, and sets the calculated size into the zippy's style
     * attribute. This is needed to allow the smooth animation work correctly,
     * as 'auto' heights cannot be used in animations.
     *
     * We also dispatch a bubbling 'measure' event, allowing content to render
     * itself only after the user requests it to be shown in the first place.
     */
    measure: function() {
      cr.dispatchSimpleEvent(this, 'measure', true);  // true = allow bubbling
      var contents = this.querySelectorAll('.zippy-content');
      for (var i = 0; i < contents.length; i++) {
        contents[i].style.height = 'auto';
      }
      // Use a separate loop to avoid relayout after each one.
      for (var i = 0; i < contents.length; i++) {
        contents[i].style.height = contents[i].offsetHeight + 'px';
      }
      // Enable animation only after setting a fixed size.
      this.classList.add('animated');
      // Collapse, disable measure mode, and then re-expand.
      this.classList.add('collapsed');
      this.classList.remove('measure');
      this.classList.remove('collapsed');
    },

    /**
     * Remeasure this zippy.
     *
     * This works by disabling animation, setting the measure property to hide
     * the zippy, letting its size auto-adjust, and then explicitly setting the
     * size to the natural size. This is required to allow smooth transitions
     * when the contents change size.
     *
     * Special care must be taken when remeasuring currently hidden zippies
     * (that is, when the entire zippy is hidden, not just when it's collapsed),
     * because the contents cannot be measured when the zippy is not displayed.
     */
    remeasure: function() {
      var contents = this.querySelectorAll('.zippy-content');

      if (this.classList.contains('collapsed')) {
        // Not currently expanded, set the measure class to remeasure later.
        this.classList.add('measure');
        // Disable animation in case it was previously measured.
        this.classList.remove('animated');
        return;
      }
      // There is still the possibility that the zippy is expanded but not
      // visible, so we will need to take special care to detect this case.

      // First disable animation and get the current heights.
      this.classList.remove('animated');
      var oldHeights = [];
      for (var i = 0; i < contents.length; i++) {
        oldHeights[i] = contents[i].style.height;
      }

      // This will hide the contents but make them resize correctly, unless they
      // are in a zippy which is itself not displayed.
      this.classList.add('remeasure');
      for (var i = 0; i < contents.length; i++) {
        contents[i].style.height = 'auto';
      }

      // Calculate the new heights and figure out if any are nonzero.
      var newHeights = [];
      var nonzero = false;
      for (var i = 0; i < contents.length; i++) {
        newHeights[i] = contents[i].offsetHeight;
        if (contents[i].offsetHeight) {
          nonzero = true;
        }
      }

      // There were no nonzero heights. We assume we're not currently showing,
      // yet expanded. Set heights to auto, leave visible, and set the content
      // to remeasure next time it is collapsed.
      if (!nonzero) {
        this.classList.remove('remeasure');
        // This function will be called before collapsing this zippy.
        this.precollapse = function() {
          for (var i = 0; i < contents.length; i++) {
            contents[i].style.height = contents[i].offsetHeight + 'px';
            var x = contents[i].offsetHeight;  // Just need to access it.
          }
          // Only now can we re-enable animation.
          this.classList.add('animated');
          this.precollapse = null;
        };
        return;
      }

      // Restore the old heights so we can animate starting from them.
      for (var i = 0; i < contents.length; i++) {
        contents[i].style.height = oldHeights[i];
      }

      // Reshow the contents and force re-layout to avoid incorrect animation.
      this.classList.remove('remeasure');
      for (var i = 0; i < contents.length; i++) {
        var x = contents[i].offsetHeight;  // Just need to access it.
      }

      // Now enable animation and set the new heights.
      this.classList.add('animated');
      for (var i = 0; i < contents.length; i++) {
        if (newHeights[i]) {
          contents[i].style.height = newHeights[i] + 'px';
        }
      }
    },

    /**
     * Returns true if this zippy is currently expanded.
     *
     * @type {boolean}
     */
    get expanded() {
      return !this.classList.contains('collapsed');
    },

    /**
     * Set the expanded state of this zippy.
     *
     * This mostly just toggles the 'collapsed' class on the zippy, but
     * it also checks for and runs other code needed to help the smooth
     * animation calculate the height of the contents correctly.
     *
     * @param {boolean} state Whether to expand the zippy.
     */
    set expanded(state) {
      if (this.expanded == state) {
        return;
      }
      if (this.precollapse && !state) {
        this.precollapse();
      }
      this.classList.toggle('collapsed');
      if (this.classList.contains('measure')) {
        this.measure();
      }
    },
  };

  // Export
  return {
    Zippy: Zippy
  };

});
