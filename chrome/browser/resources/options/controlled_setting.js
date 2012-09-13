// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Preferences = options.Preferences;

  /**
   * A controlled setting indicator that can be placed on a setting as an
   * indicator that the value is controlled by some external entity such as
   * policy or an extension.
   * @constructor
   * @extends {HTMLSpanElement}
   */
  var ControlledSettingIndicator = cr.ui.define('span');

  ControlledSettingIndicator.prototype = {
    __proto__: HTMLSpanElement.prototype,

    /**
     * Decorates the base element to show the proper icon.
     */
    decorate: function() {
      var self = this;
      var doc = self.ownerDocument;

      // Create the details and summary elements.
      var detailsContainer = doc.createElement('details');
      detailsContainer.appendChild(doc.createElement('summary'));

      // This should really create a div element, but that breaks :hover. See
      // https://bugs.webkit.org/show_bug.cgi?id=72957
      var bubbleContainer = doc.createElement('p');
      bubbleContainer.className = 'controlled-setting-bubble';
      detailsContainer.appendChild(bubbleContainer);

      self.appendChild(detailsContainer);
      self.addEventListener('click', self.show_);

      // If there is a pref, track its controlledBy property in order to be able
      // to bring up the correct bubble.
      if (this.hasAttribute('pref')) {
        Preferences.getInstance().addEventListener(
            this.getAttribute('pref'),
            function(event) {
              if (event.value) {
                var controlledBy = event.value.controlledBy;
                self.controlledBy = controlledBy ? controlledBy : null;
              }
            });

        self.resetHandler(self.clearAssociatedPref_);
      }
    },


    /**
     * Closes the bubble.
     */
    close: function() {
      this.querySelector('details').removeAttribute('open');
      this.ownerDocument.removeEventListener('click', this.closeHandler_, true);
    },

    /**
     * The given handler will be called when the user clicks on the 'reset to
     * recommended value' link shown in the indicator bubble.
     * @param {function()} handler The handler to be called.
     */
    set resetHandler(handler) {
      this.resetHandler_ = handler;
    },

    /**
     * Clears the preference associated with this indicator.
     * @private
     */
    clearAssociatedPref_: function() {
      Preferences.clearPref(this.getAttribute('pref'), this.dialogPref);
    },

    /**
     * Constructs the bubble DOM tree and shows it.
     * @private
     */
    show_: function() {
      var self = this;
      var doc = self.ownerDocument;

      // Clear out the old bubble contents.
      var bubbleContainer = this.querySelector('.controlled-setting-bubble');
      if (bubbleContainer) {
        while (bubbleContainer.hasChildNodes())
          bubbleContainer.removeChild(bubbleContainer.lastChild);
      }

      // Work out the bubble text.
      defaultStrings = {
        policy: loadTimeData.getString('controlledSettingPolicy'),
        extension: loadTimeData.getString('controlledSettingExtension'),
        recommended: loadTimeData.getString('controlledSettingRecommended'),
      };

      // No controller, no bubble.
      if (!self.controlledBy || !self.controlledBy in defaultStrings)
        return;

      var text = defaultStrings[self.controlledBy];

      // Apply text overrides.
      if (self.hasAttribute('text' + self.controlledBy))
        text = self.getAttribute('text' + self.controlledBy);

      // Create the DOM tree.
      var bubbleText = doc.createElement('p');
      bubbleText.className = 'controlled-setting-bubble-text';
      bubbleText.textContent = text;

      if (self.controlledBy == 'recommended' && self.resetHandler_) {
        var container = doc.createElement('div');
        var action = doc.createElement('button');
        action.classList.add('link-button');
        action.classList.add('controlled-setting-bubble-action');
        action.textContent =
            loadTimeData.getString('controlledSettingApplyRecommendation');
        action.addEventListener(
            'click',
            function(event) { self.resetHandler_(); });
        container.appendChild(action);
        bubbleText.appendChild(container);
      }

      bubbleContainer.appendChild(bubbleText);

      // One-time bubble-closing event handler.
      self.closeHandler_ = this.close.bind(this);
      doc.addEventListener('click', self.closeHandler_, true);
    }
  };

  /**
   * The controlling entity of the setting. Can take the values "policy",
   * "extension", "recommended" or be unset.
   */
  cr.defineProperty(ControlledSettingIndicator, 'controlledBy',
                    cr.PropertyKind.ATTR,
                    ControlledSettingIndicator.prototype.close);

  /**
   * A special preference type specific to dialogs. Changes take effect in the
   * settings UI immediately but are only actually committed when the user
   * confirms the dialog. If the user cancels the dialog instead, the changes
   * are rolled back in the settings UI and never committed.
   * @type {boolean}
   */
  cr.defineProperty(ControlledSettingIndicator, 'dialogPref',
                    cr.PropertyKind.BOOL_ATTR);

  // Export.
  return {
    ControlledSettingIndicator: ControlledSettingIndicator
  };
});
