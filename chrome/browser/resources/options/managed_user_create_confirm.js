// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * ManagedUserCreateConfirm class.
   * Encapsulated handling of the confirmation overlay page when creating a
   * managed user.
   * @constructor
   * @class
   */
  function ManagedUserCreateConfirmOverlay() {
    OptionsPage.call(this, 'managedUserCreateConfirm',
                     '',  // The title will be based on the new profile name.
                     'managed-user-created');
  };

  cr.addSingletonGetter(ManagedUserCreateConfirmOverlay);

  ManagedUserCreateConfirmOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    // Info about the newly created profile.
    profileInfo_: null,

    // Current shown slide.
    currentSlide_: 0,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('managed-user-created-done').onclick = function(event) {
        OptionsPage.closeOverlay();
      };

      var self = this;

      $('managed-user-created-switch').onclick = function(event) {
        OptionsPage.closeOverlay();
        chrome.send('switchToProfile', [self.profileInfo_.filePath]);
      };

      // Create a small dot button for each slide in the deck and make the
      // first button active.
      var numberOfSlides = $('managed-user-created-slide-deck').children.length;
      for (var i = 0; i < numberOfSlides; i++) {
        var smallButton = document.createElement('button');
        $('managed-user-created-small-buttons').appendChild(smallButton);
        smallButton.onclick = this.setCurrentSlide_.bind(this, i);
      }
      $('managed-user-created-small-buttons').children[0].classList.add(
          'managed-user-created-small-button-selected');

      // Changes the slide in |direction|, where |direction| can be 'Left' or
      // 'Right'. Changing to the left goes back in LTR and forward in RTL and
      // vice versa for right.
      function changeSlide(direction) {
        // Ignore all events we get while not visible.
        if (!self.visible)
          return;

        // Ignore anything other than left and right arrow press.
        if (direction != 'Left' && direction != 'Right')
          return;

        var container = $('managed-user-created');
        var rtl = getComputedStyle(container).direction == 'rtl';

        if ((direction == 'Right' && !rtl) || (direction == 'Left' && rtl))
          self.setCurrentSlide_(self.currentSlide_ + 1);
        else
          self.setCurrentSlide_(self.currentSlide_ - 1);
      };

      $('managed-user-created-left-slide-arrow').onclick =
          changeSlide.bind(undefined, 'Left');
      $('managed-user-created-right-slide-arrow').onclick =
          changeSlide.bind(undefined, 'Right');

      document.onkeydown = function(event) {
        changeSlide(event.keyIdentifier);
      };
    },

    /**
     * Reset to slide 1 for the next time this gets opened.
     * @override
     */
    didShowPage: function() {
      this.setCurrentSlide_(0);
    },

    /**
     * Sets the current visible slide to |slide|, where |slide| is the index
     * and starts from 0.
     * @param {number} slide The slide to set.
     * @private
     */
    setCurrentSlide_: function(slide) {
      var numberOfSlides =
          $('managed-user-created-slide-deck').children.length;
      var newSlide = (numberOfSlides + slide) % numberOfSlides;
      // Show the respective slide. The slide is shown by setting the
      // appropriate negative margin on the slide deck.
      var margin = '0';
      if (slide != 0)
        margin = '-' + newSlide * 100 + '%';
      $('managed-user-created-slide-deck').style.webkitMarginStart = margin;

      // Update the bottom buttons.
      $('managed-user-created-small-buttons').children[this.currentSlide_]
          .classList.toggle('managed-user-created-small-button-selected');
      $('managed-user-created-small-buttons').children[newSlide]
          .classList.toggle('managed-user-created-small-button-selected');
      this.currentSlide_ = newSlide;
    },

    /**
     * Sets the profile info used in the dialog and updates the profile name
     * displayed. Called by the profile creation overlay when this overlay is
     * opened.
     * @param {Object} info An object of the form:
     *     info = {
     *       name: "Profile Name",
     *       filePath: "/path/to/profile/data/on/disk"
     *       isManaged: (true|false),
     *     };
     * @private
     */
    setProfileInfo_: function(info) {
      this.profileInfo_ = info;
      $('managed-user-created-title').textContent =
          loadTimeData.getStringF('managedUserCreateConfirmTitle', info.name);
      $('managed-user-created-switch').textContent =
          loadTimeData.getStringF('managedUserCreateConfirmSwitch', info.name);
    },
  };

  // Forward public APIs to private implementations.
  [
    'setProfileInfo',
  ].forEach(function(name) {
    ManagedUserCreateConfirmOverlay[name] = function() {
      var instance = ManagedUserCreateConfirmOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManagedUserCreateConfirmOverlay: ManagedUserCreateConfirmOverlay,
  };
});
