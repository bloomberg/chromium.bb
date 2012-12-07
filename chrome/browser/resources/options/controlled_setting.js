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

      // If there is a pref, track its controlledBy and recommendedValue
      // properties in order to be able to bring up the correct bubble.
      if (this.pref) {
        Preferences.getInstance().addEventListener(
            this.pref, this.handlePrefChange.bind(this));
        this.resetHandler = this.clearAssociatedPref_;
      }

      this.className = 'controlled-setting-indicator';
      this.location = cr.ui.ArrowLocation.TOP_END;
      this.image = document.createElement('div');
      this.image.tabIndex = 0;
      this.image.setAttribute('role', 'button');
      this.image.addEventListener('click', this);
      this.image.addEventListener('keydown', this);
      this.image.addEventListener('mousedown', this);
      this.appendChild(this.image);
    },

    /**
     * The given handler will be called when the user clicks on the 'reset to
     * recommended value' link shown in the indicator bubble. The |this| object
     * will be the indicator itself.
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
      return this.image.classList.contains('showing-bubble');
    },
    set showingBubble(showing) {
      this.image.classList.toggle('showing-bubble', showing);
    },

    /**
     * Clears the preference associated with this indicator.
     * @private
     */
    clearAssociatedPref_: function() {
      Preferences.clearPref(this.pref, !this.dialogPref);
    },

    /* Handle changes to the associated pref by hiding any currently visible
     * bubble and updating the controlledBy property.
     * @param {Event} event Pref change event.
     */
    handlePrefChange: function(event) {
      OptionsPage.hideBubble();
      if (event.value.controlledBy) {
        this.controlledBy =
            !this.value || String(event.value.value) == this.value ?
            event.value.controlledBy : null;
      } else if (event.value.recommendedValue != undefined) {
        this.controlledBy =
            !this.value || String(event.value.recommendedValue) == this.value ?
            'hasRecommendation' : null;
      } else {
        this.controlledBy = null;
      }
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

        // Construct the bubble text.
        if (this.hasAttribute('plural')) {
          var defaultStrings = {
            'policy': loadTimeData.getString('controlledSettingsPolicy'),
            'extension': loadTimeData.getString('controlledSettingsExtension'),
          };
        } else {
          var defaultStrings = {
            'policy': loadTimeData.getString('controlledSettingPolicy'),
            'extension': loadTimeData.getString('controlledSettingExtension'),
            'recommended':
                loadTimeData.getString('controlledSettingRecommended'),
            'hasRecommendation':
                loadTimeData.getString('controlledSettingHasRecommendation'),
          };
        }

        // No controller, no bubble.
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

        if (this.controlledBy == 'hasRecommendation' && this.resetHandler_ &&
            !this.readOnly) {
          var container = document.createElement('div');
          var action = document.createElement('button');
          action.classList.add('link-button');
          action.classList.add('controlled-setting-bubble-action');
          action.textContent =
              loadTimeData.getString('controlledSettingFollowRecommendation');
          action.addEventListener('click', function(event) {
            self.resetHandler_();
          });
          container.appendChild(action);
          content.appendChild(container);
        }

        OptionsPage.showBubble(content, this.image, this, this.location);
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
   * The value of the associated preference that the indicator represents. If
   * this is not set, the indicator will be visible whenever any value is
   * enforced or recommended. If it is set, the indicator will be visible only
   * when the enforced or recommended value matches the value it represents.
   * This allows multiple indicators to be created for a set of radio buttons,
   * ensuring that only one of them is visible at a time.
   */
  cr.defineProperty(ControlledSettingIndicator, 'value',
                    cr.PropertyKind.ATTR);

  /**
   * The status of the associated preference:
   * - 'policy':            A specific value is enfoced by policy.
   * - 'extension':         A specific value is enforced by an extension.
   * - 'recommended':       A value is recommended by policy. The user could
   *                        override this recommendation but has not done so.
   * - 'hasRecommendation': A value is recommended by policy. The user has
   *                        overridden this recommendation.
   * - unset:               The value is controlled by the user alone.
   * @type {string}
   */
  cr.defineProperty(ControlledSettingIndicator, 'controlledBy',
                    cr.PropertyKind.ATTR);

  // Export.
  return {
    ControlledSettingIndicator: ControlledSettingIndicator
  };
});
