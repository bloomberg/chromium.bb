// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var Preferences = options.Preferences;

  /**
   * Helper function update element's state from pref change event.
   * @private
   * @param {!HTMLElement} el The element to update.
   * @param {!Event} event The pref change event.
   */
  function updateElementState_(el, event) {
    el.managed = event.value && event.value['managed'] != undefined ?
        event.value['managed'] : false;

    // Managed UI elements can only be disabled as a result of being
    // managed. They cannot be enabled as a result of a pref being
    // unmanaged.
    if (el.managed)
      el.disabled = true;

    // Disable UI elements if backend says so.
    if (!el.disabled && event.value && event.value['disabled'])
      el.disabled = true;
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

      self.initializeValueType(self.getAttribute('value-type'));

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(
          this.pref,
          function(event) {
            var value = event.value && event.value['value'] != undefined ?
                event.value['value'] : event.value;

            // Invert pref value if inverted_pref == true.
            if (self.inverted_pref)
              self.checked = !Boolean(value);
            else
              self.checked = Boolean(value);

            updateElementState_(self, event);
          });

      // Listen to user events.
      this.addEventListener(
          'change',
          function(e) {
            var value = self.inverted_pref ? !self.checked : self.checked;
            switch(self.valueType) {
              case 'number':
                Preferences.setIntegerPref(self.pref,
                    Number(value), self.metric);
                break;
              case 'boolean':
                Preferences.setBooleanPref(self.pref,
                    value, self.metric);
                break;
            }
          });
    },

    /**
     * Sets up options in checkbox element.
     * @param {String} valueType The preference type for this checkbox.
     */
    initializeValueType: function(valueType) {
      this.valueType = valueType || 'boolean';
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefCheckbox, 'pref', cr.PropertyKind.ATTR);

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

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'radio';
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            var value = event.value && event.value['value'] != undefined ?
                event.value['value'] : event.value;
            self.checked = String(value) == self.value;

            updateElementState_(self, event);
          });

      // Listen to user events.
      // Use the 'click' event instead of 'change', because of a bug in WebKit
      // which prevents 'change' from being sent when the user changes selection
      // using the keyboard.
      // https://bugs.webkit.org/show_bug.cgi?id=32013
      this.addEventListener('click',
          function(e) {
            if(self.value == 'true' || self.value == 'false') {
              Preferences.setBooleanPref(self.pref,
                  self.value == 'true', self.metric);
            } else {
              Preferences.setIntegerPref(self.pref,
                  parseInt(self.value, 10), self.metric);
            }
          });
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefRadio, 'pref', cr.PropertyKind.ATTR);

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
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.value = event.value && event.value['value'] != undefined ?
                event.value['value'] : event.value;

            updateElementState_(self, event);
          });

      // Listen to user events.
      this.addEventListener('change',
          function(e) {
            if (this.validity.valid) {
              Preferences.setIntegerPref(self.pref, self.value, self.metric);
            }
          });
    }
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefNumeric, 'pref', cr.PropertyKind.ATTR);

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

      // Listen to user events.
      this.addEventListener('input',
          function(e) {
            if (this.validity.valid) {
              Preferences.setIntegerPref(self.pref, self.value, self.metric);
            }
          });
    }
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
     * If true, the associated pref will be modified on each onchange event;
     * otherwise, the pref will only be modified on the onmouseup event after
     * the drag.
     */
    continuous: true,

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
      this.onchange = this.onChange_.bind(this);
      this.onkeyup = this.onmouseup = this.onInputUp_.bind(this);
    },

    /**
     * Event listener that updates the UI when the underlying pref changes.
     * @param {Event} event The event that details the pref change.
     * @private
     */
    onPrefChange_: function(event) {
      var value = event.value && event.value['value'] != undefined ?
          event.value['value'] : event.value;
      if (value != undefined)
        this.value = this.valueMap ? this.valueMap.indexOf(value) : value;
    },

    /**
     * onchange handler that sets the pref when the user changes the value of
     * the input element.
     * @private
     */
    onChange_: function(event) {
      if (this.continuous)
        this.setRangePref_();

      if (this.notifyChange)
        this.notifyChange(this, this.mapValueToRange_(this.value));
    },

    /**
     * Sets the integer value of |pref| to the value of this element.
     * @private
     */
    setRangePref_: function() {
      Preferences.setIntegerPref(
          this.pref, this.mapValueToRange_(this.value), this.metric);

      if (this.notifyPrefChange)
        this.notifyPrefChange(this, this.mapValueToRange_(this.value));
    },

    /**
     * onkeyup/onmouseup handler that modifies the pref if |continuous| is
     * false.
     * @private
     */
    onInputUp_: function(event) {
      if (!this.continuous)
        this.setRangePref_();
    },

    /**
     * Maps the value of this element into the range provided by the client,
     * represented by |valueMap|.
     * @param {number} value The value to map.
     * @private
     */
    mapValueToRange_: function(value) {
      return this.valueMap ? this.valueMap[value] : value;
    },

    /**
     * Called when the client has specified non-continuous mode and the value of
     * the range control changes.
     * @param {Element} el This element.
     * @param {number} value The value of this element.
     */
    notifyChange: function(el, value) {
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefRange, 'pref', cr.PropertyKind.ATTR);

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
      var self = this;

      // Listen to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            var value = event.value && event.value['value'] != undefined ?
                event.value['value'] : event.value;

            // Make sure |value| is a string, because the value is stored as a
            // string in the HTMLOptionElement.
            value = value.toString();

            updateElementState_(self, event);

            var found = false;
            for (var i = 0; i < self.options.length; i++) {
              if (self.options[i].value == value) {
                self.selectedIndex = i;
                found = true;
              }
            }

            // Item not found, select first item.
            if (!found)
              self.selectedIndex = 0;

            if (self.onchange != undefined)
              self.onchange(event);
          });

      // Listen to user events.
      this.addEventListener('change',
          function(e) {
            if (!self.dataType) {
              console.error('undefined data type for <select> pref');
              return;
            }

            switch(self.dataType) {
              case 'number':
                Preferences.setIntegerPref(self.pref,
                    self.options[self.selectedIndex].value, self.metric);
                break;
              case 'double':
                Preferences.setDoublePref(self.pref,
                    self.options[self.selectedIndex].value, self.metric);
                break;
              case 'boolean':
                var option = self.options[self.selectedIndex];
                var value = (option.value == 'true') ? true : false;
                Preferences.setBooleanPref(self.pref, value, self.metric);
                break;
              case 'string':
                Preferences.setStringPref(self.pref,
                    self.options[self.selectedIndex].value, self.metric);
                break;
              default:
                console.error('unknown data type for <select> pref: ' +
                              self.dataType);
            }
          });
    },
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefSelect, 'pref', cr.PropertyKind.ATTR);

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
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.value = event.value && event.value['value'] != undefined ?
                event.value['value'] : event.value;

            updateElementState_(self, event);
          });

      // Listen to user events.
      this.addEventListener('change',
          function(e) {
            switch(self.dataType) {
              case 'number':
                Preferences.setIntegerPref(self.pref, self.value, self.metric);
                break;
              case 'double':
                Preferences.setDoublePref(self.pref, self.value, self.metric);
                break;
              default:
                Preferences.setStringPref(self.pref, self.value, self.metric);
                break;
            }
          });

      window.addEventListener('unload',
          function() {
            if (document.activeElement == self)
              self.blur();
          });
    }
  };

  /**
   * The preference name.
   * @type {string}
   */
  cr.defineProperty(PrefTextField, 'pref', cr.PropertyKind.ATTR);

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

  // Export
  return {
    PrefCheckbox: PrefCheckbox,
    PrefNumber: PrefNumber,
    PrefNumeric: PrefNumeric,
    PrefRadio: PrefRadio,
    PrefRange: PrefRange,
    PrefSelect: PrefSelect,
    PrefTextField: PrefTextField
  };

});
