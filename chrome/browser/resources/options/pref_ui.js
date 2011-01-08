// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var Preferences = options.Preferences;
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

            self.managed = event.value && event.value['managed'] != undefined ?
                event.value['managed'] : false;

            // Managed UI elements can only be disabled as a result of being
            // managed. They cannot be enabled as a result of a pref being
            // unmanaged.
            if (self.managed)
              self.disabled = true;
          });

      // Listen to user events.
      this.addEventListener(
          'click',
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
    }
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
            self.managed = event.value && event.value['managed'] != undefined ?
                event.value['managed'] : false;
            self.checked = String(value) == self.value;

            // Managed UI elements can only be disabled as a result of being
            // managed. They cannot be enabled as a result of a pref being
            // unmanaged.
            if (self.managed)
              self.disabled = true;
          });

      // Listen to user events.
      this.addEventListener('change',
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

    /**
     * Getter for preference name attribute.
     */
    get pref() {
      return this.getAttribute('pref');
    },

    /**
     * Setter for preference name attribute.
     */
    set pref(name) {
      this.setAttribute('pref', name);
    }
  };

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
            self.managed = event.value && event.value['managed'] != undefined ?
                event.value['managed'] : false;

            // Managed UI elements can only be disabled as a result of being
            // managed. They cannot be enabled as a result of a pref being
            // unmanaged.
            if (self.managed)
              self.disabled = true;
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
    __proto__: PrefNumeric.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'range';
      PrefNumeric.prototype.decorate.call(this);
      var self = this;

      // Additionally change the indicator as well.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.updateIndicator();
          });

      // Listen to user events.
      this.addEventListener('input',
          function(e) {
            this.updateIndicator();
          });
    },

    updateIndicator: function() {
      if ($(this.id + '-value')) {
        $(this.id + '-value').textContent = this.value;
      }
    }
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefSelect class:

  // Define a constructor that uses an select element as its underlying element.
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

            self.managed = event.value && event.value['managed'] != undefined ?
                event.value['managed'] : false;

            // Managed UI elements can only be disabled as a result of being
            // managed. They cannot be enabled as a result of a pref being
            // unmanaged.
            if (self.managed)
              self.disabled = true;

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
            switch(self.dataType) {
              case 'number':
                Preferences.setIntegerPref(self.pref,
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
            self.managed = event.value && event.value['managed'] != undefined ?
                event.value['managed'] : false;

            // Managed UI elements can only be disabled as a result of being
            // managed. They cannot be enabled as a result of a pref being
            // unmanaged.
            if (self.managed)
              self.disabled = true;
          });

      // Listen to user events.
      this.addEventListener('change',
          function(e) {
            Preferences.setStringPref(self.pref, self.value, self.metric);
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
