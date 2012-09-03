// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var Preferences = options.Preferences;

  /**
   * Allows an element to be disabled for several reasons.
   * The element is disabled if at least one reason is true, and the reasons
   * can be set separately.
   * @private
   * @param {!HTMLElement} el The element to update.
   * @param {string} reason The reason for disabling the element.
   * @param {boolean} disabled Whether the element should be disabled or enabled
   * for the given |reason|.
   */
  function updateDisabledState_(el, reason, disabled) {
    if (!el.disabledReasons)
      el.disabledReasons = {};
    if (el.disabled && (Object.keys(el.disabledReasons).length == 0)) {
      // The element has been previously disabled without a reason, so we add
      // one to keep it disabled.
      el.disabledReasons['other'] = true;
    }
    if (!el.disabled) {
      // If the element is not disabled, there should be no reason, except for
      // 'other'.
      delete el.disabledReasons['other'];
      if (Object.keys(el.disabledReasons).length > 0)
        console.error('Element is not disabled but should be');
    }
    if (disabled) {
      el.disabledReasons[reason] = true;
    } else {
      delete el.disabledReasons[reason];
    }
    el.disabled = Object.keys(el.disabledReasons).length > 0;
  }

  /**
   * Helper function to update element's state from pref change event.
   * @private
   * @param {!HTMLElement} el The element to update.
   * @param {!Event} event The pref change event.
   */
  function updateElementState_(el, event) {
    updateDisabledState_(el, 'notUserModifiable', event.value.disabled);
    el.controlledBy = event.value.controlledBy;
    OptionsPage.updateManagedBannerVisibility();
  }

  /////////////////////////////////////////////////////////////////////////////
  // PrefCheckbox class:
  // TODO(jhawkins): Refactor all this copy-pasted code!

  // Define a constructor that uses an input element as its underlying element.
  var PrefCheckbox = cr.ui.define('input');

  PrefCheckbox.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'checkbox';
      var self = this;

      this.initializeValueType(this.getAttribute('value-type'));

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref, function(event) {
        var value = event.value.value;
        self.checked = self.inverted_pref ? !value : value;
        updateElementState_(self, event);
      });

      // Listen to user events.
      this.addEventListener('change', this.updatePreferenceValue_.bind(this));
    },

    /**
     * Handler that updates the associated pref when the user changes the input
     * element's value.
     * @param {Event} event Change event.
     * @private
     */
    updatePreferenceValue_: function(event) {
      if (this.customChangeHandler(event))
        return;

      var value = this.inverted_pref ? !this.checked : this.checked;
      switch (this.valueType) {
        case 'number':
          Preferences.setIntegerPref(this.pref, Number(value),
                                     !this.dialogPref, this.metric);
          break;
        case 'boolean':
          Preferences.setBooleanPref(this.pref, value,
                                     !this.dialogPref, this.metric);
          break;
      }
    },

    /**
     * Sets up options in checkbox element.
     * @param {String} valueType The preference type for this checkbox.
     */
    initializeValueType: function(valueType) {
      this.valueType = valueType || 'boolean';
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },

    /**
     * Handler that is called when the user changes the input element's value.
     * If it returns false, the default handler is executed next, updating the
     * associated pref to its new value. If it returns true, the default handler
     * is skipped (i.e., this works like stopPropagation or cancelBubble).
     * @param {Event} event Change event.
     */
    customChangeHandler: function(event) {
      return false;
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefCheckbox, 'pref', cr.PropertyKind.ATTR);

  /**
   * A special preference type specific to dialogs. Changes take effect in the
   * settings UI immediately but are only actually committed when the user
   * confirms the dialog. If the user cancels the dialog instead, the changes
   * are rolled back in the settings UI and never committed.
   * @type {boolean}
   */
  cr.defineProperty(PrefCheckbox, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the preference is controlled by something else than the user's
   * settings (either 'policy' or 'extension').
   * @type {string}
   */
  cr.defineProperty(PrefCheckbox, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * The user metric string.
   * @type {string}
   */
  cr.defineProperty(PrefCheckbox, 'metric', cr.PropertyKind.ATTR);

  /**
   * Whether to use inverted pref value.
   * @type {boolean}
   */
  cr.defineProperty(PrefCheckbox, 'inverted_pref', cr.PropertyKind.BOOL_ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefRadio class:

  //Define a constructor that uses an input element as its underlying element.
  var PrefRadio = cr.ui.define('input');

  PrefRadio.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    // Stores the initialized value of the preference used to reset the input
    // in resetPrefState().
    storedValue_: null,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'radio';
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref, function(event) {
        self.checked = String(event.value.value) == self.value;
        updateElementState_(self, event);
      });

      // Listen to user events.
      this.addEventListener('change', this.updatePreferenceValue_.bind(this));
    },

    /**
     * Handler that updates the associated pref when the user changes the input
     * element's value.
     * @param {Event} event Change event.
     * @private
     */
    updatePreferenceValue_: function(event) {
      if (this.customChangeHandler(event))
        return;

      if (this.value == 'true' || this.value == 'false') {
        var value = String(this.value);
        Preferences.setBooleanPref(this.pref, value == String(this.checked),
                                   !this.dialogPref, this.metric);
      } else {
        Preferences.setIntegerPref(this.pref, parseInt(this.value, 10),
                                   !this.dialogPref, this.metric);
      }
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },

    /**
     * Handler that is called when the user changes the input element's value.
     * If it returns false, the default handler is executed next, updating the
     * associated pref to its new value. If it returns true, the default handler
     * is skipped (i.e., this works like stopPropagation or cancelBubble).
     * @param {Event} event Change event.
     */
    customChangeHandler: function(event) {
      return false;
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefRadio, 'pref', cr.PropertyKind.ATTR);

  /**
   * A special preference type specific to dialogs. Changes take effect in the
   * settings UI immediately but are only actually committed when the user
   * confirms the dialog. If the user cancels the dialog instead, the changes
   * are rolled back in the settings UI and never committed.
   * @type {boolean}
   */
  cr.defineProperty(PrefRadio, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the preference is controlled by something else than the user's
   * settings (either 'policy' or 'extension').
   * @type {string}
   */
  cr.defineProperty(PrefRadio, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * The user metric string.
   * @type {string}
   */
  cr.defineProperty(PrefRadio, 'metric', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefNumeric class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefNumeric = function() {};
  PrefNumeric.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref, function(event) {
        self.value = event.value.value;
        updateElementState_(self, event);
      });

      // Listen to user events.
      this.addEventListener('change', function(event) {
        if (self.validity.valid) {
          Preferences.setIntegerPref(self.pref, self.value,
                                     !self.dialogPref, self.metric);
        }
      });
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefNumeric, 'pref', cr.PropertyKind.ATTR);

  /**
   * A special preference type specific to dialogs. Changes take effect in the
   * settings UI immediately but are only actually committed when the user
   * confirms the dialog. If the user cancels the dialog instead, the changes
   * are rolled back in the settings UI and never committed.
   * @type {boolean}
   */
  cr.defineProperty(PrefRadio, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the preference is controlled by something else than the user's
   * settings (either 'policy' or 'extension').
   * @type {string}
   */
  cr.defineProperty(PrefNumeric, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * The user metric string.
   * @type {string}
   */
  cr.defineProperty(PrefNumeric, 'metric', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefNumber class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefNumber = cr.ui.define('input');

  PrefNumber.prototype = {
    // Set up the prototype chain
    __proto__: PrefNumeric.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'number';
      PrefNumeric.prototype.decorate.call(this);
      var self = this;

      // Listen to user events.
      this.addEventListener('input',
          function(e) {
            if (self.validity.valid) {
              Preferences.setIntegerPref(self.pref, self.value,
                                         !self.dialogPref, self.metric);
            }
          });
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefRange class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefRange = cr.ui.define('input');

  PrefRange.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * The map from input range value to the corresponding preference value.
     */
    valueMap: undefined,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'range';

      // Update the UI when the pref changes.
      Preferences.getInstance().addEventListener(
          this.pref, this.onPrefChange_.bind(this));

      // Listen to user events.
      // TODO(jhawkins): Add onmousewheel handling once the associated WK bug is
      // fixed.
      // https://bugs.webkit.org/show_bug.cgi?id=52256
      this.addEventListener('keyup', this.updatePreferenceValue_.bind(this));
      this.addEventListener('mouseup', this.updatePreferenceValue_.bind(this));
    },

    /**
     * Event listener that updates the input element when the associated pref
     * changes.
     * @param {Event} event The event that details the pref change.
     * @private
     */
    onPrefChange_: function(event) {
      var value = event.value.value;
      this.value = this.valueMap ? this.valueMap.indexOf(value) : value;
    },

    /**
     * Handler that updates the associated pref when the user changes the input
     * element's value.
     * This handler is called when the user completes the change by releasing
     * the slider.
     * @param {Event} event Change event.
     * @private
     */
    updatePreferenceValue_: function(event) {
      if (this.customChangeHandler(event))
        return;

      Preferences.setIntegerPref(this.pref, this.mapValueToRange(this.value),
                                 !this.dialogPref, this.metric);
    },

    /**
     * Maps the value of this element into the range provided by the client,
     * represented by |valueMap|.
     * @param {number} value The value to map.
     */
    mapValueToRange: function(value) {
      return this.valueMap ? this.valueMap[value] : value;
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },

    /**
     * Handler that is called when the user changes the input element's value.
     * If it returns false, the default handler is executed next, updating the
     * associated pref to its new value. If it returns true, the default handler
     * is skipped (i.e., this works like stopPropagation or cancelBubble).
     * This handler is called when the user completes the change by releasing
     * the slider.
     * @param {Event} event Change event.
     */
    customChangeHandler: function(event) {
      return false;
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefRange, 'pref', cr.PropertyKind.ATTR);

  /**
   * A special preference type specific to dialogs. Changes take effect in the
   * settings UI immediately but are only actually committed when the user
   * confirms the dialog. If the user cancels the dialog instead, the changes
   * are rolled back in the settings UI and never committed.
   * @type {boolean}
   */
  cr.defineProperty(PrefRadio, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the preference is controlled by something else than the user's
   * settings (either 'policy' or 'extension').
   * @type {string}
   */
  cr.defineProperty(PrefRange, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * The user metric string.
   * @type {string}
   */
  cr.defineProperty(PrefRange, 'metric', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefSelect class:

  // Define a constructor that uses a select element as its underlying element.
  var PrefSelect = cr.ui.define('select');

  PrefSelect.prototype = {
    // Set up the prototype chain
    __proto__: HTMLSelectElement.prototype,

    /**
    * Initialization function for the cr.ui framework.
    */
    decorate: function() {
      // Listen to pref changes.
      Preferences.getInstance().addEventListener(
          this.pref, this.onPrefChange_.bind(this));

      // Listen to user events.
      this.addEventListener('change', this.updatePreferenceValue_.bind(this));
    },

    /**
     * Event listener that updates the input element when the associated pref
     * changes.
     * @param {Event} event The event that details the pref change.
     * @private
     */
    onPrefChange_: function(event) {
      // Make sure |value| is a string, because the value is stored as a string
      // in the HTMLOptionElement.
      var value = event.value.value.toString();

      updateElementState_(this, event);

      var found = false;
      for (var i = 0; i < this.options.length; i++) {
        if (this.options[i].value == value) {
          this.selectedIndex = i;
          found = true;
        }
      }

      // Item not found, select first item.
      if (!found)
        this.selectedIndex = 0;

      if (this.onchange)
        this.onchange(event);
    },

    /**
     * Handler that updates the associated pref when the user changes the input
     * element's value.
     * @param {Event} event Change event.
     * @private
     */
    updatePreferenceValue_: function(event) {
      if (!this.dataType) {
        console.error('undefined data type for <select> pref');
        return;
      }

      var prefValue = this.options[this.selectedIndex].value;
      switch (this.dataType) {
        case 'number':
          Preferences.setIntegerPref(this.pref, prefValue,
                                     !this.dialogPref, this.metric);
          break;
        case 'double':
          Preferences.setDoublePref(this.pref, prefValue,
                                    !this.dialogPref, this.metric);
          break;
        case 'boolean':
          var value = (prefValue == 'true');
          Preferences.setBooleanPref(this.pref, value,
                                     !this.dialogPref, this.metric);
          break;
        case 'string':
          Preferences.setStringPref(this.pref, prefValue,
                                    !this.dialogPref, this.metric);
          break;
        default:
          console.error('unknown data type for <select> pref: ' +
                        this.dataType);
      }
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefSelect, 'pref', cr.PropertyKind.ATTR);

  /**
   * Whether the preference is controlled by something else than the user's
   * settings (either 'policy' or 'extension').
   * @type {string}
   */
  cr.defineProperty(PrefSelect, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * A special preference type specific to dialogs. Changes take effect in the
   * settings UI immediately but are only actually committed when the user
   * confirms the dialog. If the user cancels the dialog instead, the changes
   * are rolled back in the settings UI and never committed.
   * @type {boolean}
   */
  cr.defineProperty(PrefSelect, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * The user metric string.
   * @type {string}
   */
  cr.defineProperty(PrefSelect, 'metric', cr.PropertyKind.ATTR);

  /**
   * The data type for the preference options.
   * @type {string}
   */
  cr.defineProperty(PrefSelect, 'dataType', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefTextField class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefTextField = cr.ui.define('input');

  PrefTextField.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref, function(event) {
        self.value = event.value.value;
        updateElementState_(self, event);
      });

      // Listen to user events.
      this.addEventListener('change', this.updatePreferenceValue_.bind(this));

      window.addEventListener('unload', function(event) {
        if (document.activeElement == self)
          self.blur();
      });
    },

    /**
     * Handler that updates the associated pref when the user changes the input
     * element's value.
     * @param {Event} event Change event.
     * @private
     */
    updatePreferenceValue_: function(event) {
      switch (this.dataType) {
        case 'number':
          Preferences.setIntegerPref(this.pref, this.value,
                                     !this.dialogPref, this.metric);
          break;
        case 'double':
          Preferences.setDoublePref(this.pref, this.value,
                                    !this.dialogPref, this.metric);
          break;
        case 'url':
          Preferences.setURLPref(this.pref, this.value,
                                 !this.dialogPref, this.metric);
          break;
        default:
          Preferences.setStringPref(this.pref, this.value,
                                    !this.dialogPref, this.metric);
          break;
      }
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefTextField, 'pref', cr.PropertyKind.ATTR);

  /**
   * A special preference type specific to dialogs. Changes take effect in the
   * settings UI immediately but are only actually committed when the user
   * confirms the dialog. If the user cancels the dialog instead, the changes
   * are rolled back in the settings UI and never committed.
   * @type {boolean}
   */
  cr.defineProperty(PrefTextField, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the preference is controlled by something else than the user's
   * settings (either 'policy' or 'extension').
   * @type {string}
   */
  cr.defineProperty(PrefTextField, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * The user metric string.
   * @type {string}
   */
  cr.defineProperty(PrefTextField, 'metric', cr.PropertyKind.ATTR);

  /**
   * The data type for the preference options.
   * @type {string}
   */
  cr.defineProperty(PrefTextField, 'dataType', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefButton class:

  // Define a constructor that uses a button element as its underlying element.
  var PrefButton = cr.ui.define('button');

  PrefButton.prototype = {
    // Set up the prototype chain
    __proto__: HTMLButtonElement.prototype,

    /**
    * Initialization function for the cr.ui framework.
    */
    decorate: function() {
      var self = this;

      // Listen to pref changes. This element behaves like a normal button and
      // doesn't affect the underlying preference; it just becomes disabled
      // when the preference is managed, and its value is false.
      // This is useful for buttons that should be disabled when the underlying
      // boolean preference is set to false by a policy or extension.
      Preferences.getInstance().addEventListener(this.pref, function(event) {
        var e = {
          value: {
            'disabled': event.value.disabled && !event.value.value,
            'controlledBy': event.value.controlledBy
          }
        };
        updateElementState_(self, e);
      });
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefButton, 'pref', cr.PropertyKind.ATTR);

  /**
   * Whether the preference is controlled by something else than the user's
   * settings (either 'policy' or 'extension').
   * @type {string}
   */
  cr.defineProperty(PrefButton, 'controlledBy', cr.PropertyKind.ATTR);

  // Export
  return {
    PrefCheckbox: PrefCheckbox,
    PrefNumber: PrefNumber,
    PrefNumeric: PrefNumeric,
    PrefRadio: PrefRadio,
    PrefRange: PrefRange,
    PrefSelect: PrefSelect,
    PrefTextField: PrefTextField,
    PrefButton: PrefButton
  };

});
