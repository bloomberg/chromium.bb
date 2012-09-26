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

      // If there is a pref, track its controlledBy property in order to be able
      // to bring up the correct bubble.
      if (this.pref) {
        Preferences.getInstance().addEventListener(this.pref,
            function(event) {
              var controlledBy = event.value.controlledBy;
              self.controlledBy = controlledBy ? controlledBy : null;
              OptionsPage.hideBubble();
            });
        this.resetHandler = this.clearAssociatedPref_;
      }

      this.tabIndex = 0;
      this.setAttribute('role', 'button');
      this.addEventListener('click', this);
      this.addEventListener('keydown', this);
      this.addEventListener('mousedown', this);
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
     * Whether the indicator is currently showing a bubble.
     * @type {boolean}
     */
    get showingBubble() {
      return !!this.showingBubble_;
    },
    set showingBubble(showing) {
      if (showing)
        this.classList.add('showing-bubble');
      else
        this.classList.remove('showing-bubble');
      this.showingBubble_ = showing;
    },

    /**
     * Clears the preference associated with this indicator.
     * @private
     */
    clearAssociatedPref_: function() {
      Preferences.clearPref(this.pref, this.dialogPref);
    },

    /**
     * Handle mouse and keyboard events, allowing the user to open and close a
     * bubble with further information.
     * @param {Event} event Mouse or keyboard event.
     */
    handleEvent: function(event) {
      switch (event.type) {
        // Toggle the bubble on left click. Let any other clicks propagate.
        case 'click':
          if (event.button != 0)
            return;
          break;
        // Toggle the bubble when <Return> or <Space> is pressed. Let any other
        // key presses propagate.
        case 'keydown':
          switch (event.keyCode) {
            case 13:  // Return.
            case 32:  // Space.
              break;
            default:
              return;
          }
          break;
        // Blur focus when a mouse button is pressed, matching the behavior of
        // other Web UI elements.
        case 'mousedown':
          if (document.activeElement)
            document.activeElement.blur();
          event.preventDefault();
          return;
      }
      this.toggleBubble_();
      event.preventDefault();
      event.stopPropagation();
    },

    /**
     * Open or close a bubble with further information about the pref.
     * @private
     */
    toggleBubble_: function() {
      if (this.showingBubble) {
        OptionsPage.hideBubble();
      } else {
        var self = this;

        // Work out the popup text.
        defaultStrings = {
          'policy': loadTimeData.getString('controlledSettingPolicy'),
          'extension': loadTimeData.getString('controlledSettingExtension'),
          'recommended': loadTimeData.getString('controlledSettingRecommended'),
        };

        // No controller, no popup.
        if (!this.controlledBy || !(this.controlledBy in defaultStrings))
          return;

        var text = defaultStrings[this.controlledBy];

        // Apply text overrides.
        if (this.hasAttribute('text' + this.controlledBy))
          text = this.getAttribute('text' + this.controlledBy);

        // Create the DOM tree.
        var content = document.createElement('div');
        content.className = 'controlled-setting-bubble-content';
        content.setAttribute('controlled-by', this.controlledBy);
        content.textContent = text;

        if (this.controlledBy == 'recommended' && this.resetHandler_) {
          var container = document.createElement('div');
          var action = document.createElement('button');
          action.classList.add('link-button');
          action.classList.add('controlled-setting-bubble-action');
          action.textContent =
              loadTimeData.getString('controlledSettingApplyRecommendation');
          action.addEventListener('click', function(event) {
            self.resetHandler_();
          });
          container.appendChild(action);
          content.appendChild(container);
        }

        OptionsPage.showBubble(content, this);
      }
    },
  };

  /**
   * The name of the associated preference.
   * @type {string}
   */
  cr.defineProperty(ControlledSettingIndicator, 'pref', cr.PropertyKind.ATTR);

  /**
   * Whether this indicator is part of a dialog. If so, changes made to the
   * associated preference take effect in the settings UI immediately but are
   * only actually committed when the user confirms the dialog. If the user
   * cancels the dialog instead, the changes are rolled back in the settings UI
   * and never committed.
   * @type {boolean}
   */
  cr.defineProperty(ControlledSettingIndicator, 'dialogPref',
                    cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the associated preference is controlled by a source other than the
   * user's setting (can be 'policy', 'extension', 'recommended' or unset).
   * @type {string}
   */
  cr.defineProperty(ControlledSettingIndicator, 'controlledBy',
                    cr.PropertyKind.ATTR);

  // Export.
  return {
    ControlledSettingIndicator: ControlledSettingIndicator
  };
});
