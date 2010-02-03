// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const cr = (function() {

  /**
   * Whether we are using a Mac or not.
   * @type {boolean}
   */
  const isMac = /Mac/.test(navigator.platform);

  /**
   * Builds an object structure for the provided namespace path,
   * ensuring that names that already exist are not overwritten. For
   * example:
   * "a.b.c" -> a = {};a.b={};a.b.c={};
   * @param {string} name Name of the object that this file defines.
   * @param {*=} opt_object The object to expose at the end of the path.
   * @param {Object=} opt_objectToExportTo The object to add the path to;
   *     default is {@code window}.
   * @private
   */
  function exportPath(name, opt_object, opt_objectToExportTo) {
    var parts = name.split('.');
    var cur = opt_objectToExportTo || window /* global */;

    for (var part; parts.length && (part = parts.shift());) {
      if (!parts.length && opt_object !== undefined) {
        // last part and we have an object; use it
        cur[part] = opt_object;
      } else if (part in cur) {
        cur = cur[part];
      } else {
        cur = cur[part] = {};
      }
    }
    return cur;
  };

  /**
   * Fires a property change event on the target.
   * @param {EventTarget} target The target to dispatch the event on.
   * @param {string} propertyName The name of the property that changed.
   * @param {*} newValue The new value for the property.
   * @param {*} oldValue The old value for the property.
   */
  function dispatchPropertyChange(target, propertyName, newValue, oldValue) {
    // TODO(arv): Depending on cr.Event here is a bit ugly.
    var e = new cr.Event(propertyName + 'Change');
    e.propertyName = propertyName;
    e.newValue = newValue;
    e.oldValue = oldValue;
    target.dispatchEvent(e);
  }

  /**
   * The kind of property to define in {@code defineProperty}.
   * @enum {number}
   */
  const PropertyKind = {
    /**
     * Plain old JS property where the backing data is stored as a "private"
     * field on the object.
     */
    JS: 'js',

    /**
     * The property backing data is stored as an attribute on an element.
     */
    ATTR: 'attr',

    /**
     * The property backing data is stored as an attribute on an element. If the
     * element has the attribute then the value is true.
     */
    BOOL_ATTR: 'boolAttr'
  };

  /**
   * Helper function for defineProperty that returns the getter to use for the
   * property.
   * @param {string} name
   * @param {cr.PropertyKind} kind
   * @param {*} defaultValue The default value. This is only used for the ATTR
   *    kind.
   * @return {function():*} The getter for the property.
   */
  function getGetter(name, kind, defaultValue) {
    switch (kind) {
      case PropertyKind.JS:
        var privateName = name + '_';
        return function() {
          return this[privateName];
        };
      case PropertyKind.ATTR:
        // For attr with default value we return the default value if the
        // element is missing the attribute.
        if (defaultValue == undefined) {
          return function() {
            return this.getAttribute(name);
          };
        } else {
          return function() {
            // WebKit uses null for non existant attributes.
            var value = this.getAttribute(name);
            return value !== null ? value : defaultValue;
          };
        }
      case PropertyKind.BOOL_ATTR:
        // Boolean attributes don't support default values.
        return function() {
          return this.hasAttribute(name);
        };
    }
  }

  /**
   * Helper function for defineProperty that returns the setter of the right
   * kind.
   * @param {string} name The name of the property we are defining the setter
   *     for.
   * @param {cr.PropertyKind} kind The kind of property we are getting the
   *     setter for.
   * @return {function(*):void} The function to use as a setter.
   */
  function getSetter(name, kind) {
    switch (kind) {
      case PropertyKind.JS:
        var privateName = name + '_';
        return function(value) {
          var oldValue = this[privateName];
          if (value !== oldValue) {
            this[privateName] = value;
            dispatchPropertyChange(this, name, value, oldValue);
          }
        };

      case PropertyKind.ATTR:
        return function(value) {
          var oldValue = this[name];
          if (value !== oldValue) {
            this.setAttribute(name, value);
            dispatchPropertyChange(this, name, value, oldValue);
          }
        };

      case PropertyKind.BOOL_ATTR:
        return function(value) {
          var oldValue = this[name];
          if (value !== oldValue) {
            if (value)
              this.setAttribute(name, name);
            else
              this.removeAttribute(name);
            dispatchPropertyChange(this, name, value, oldValue);
          }
        };
    }
  }

  /**
   * Defines a property on an object. When the setter changes the value a
   * property change event with the type {@code name + 'Change'} is fired.
   * @param {!Object} The object to define the property for.
   * @param {string} The name of the property.
   * @param {cr.PropertyKind=} opt_kind What kind of underlying storage to use.
   * @param {*} opt_defaultValue The default value.
   */
  function defineProperty(obj, name, opt_kind, opt_default) {
    if (typeof obj == 'function')
      obj = obj.prototype;

    var kind = opt_kind || PropertyKind.JS;

    if (!obj.__lookupGetter__(name)) {
      // For js properties we set the default value on the prototype.
      if (kind == PropertyKind.JS && arguments.length > 3)  {
        var privateName = name + '_';
        obj[privateName] = opt_default;
      }
      obj.__defineGetter__(name, getGetter(name, kind, opt_default));
    }

    if (!obj.__lookupSetter__(name)) {
      obj.__defineSetter__(name, getSetter(name, kind));
    }
  }

  /**
   * Counter for use with createUid
   */
  var uidCounter = 1;

  /**
   * @return {number} A new unique ID.
   */
  function createUid() {
    return uidCounter++;
  }

  /**
   * Returns a unique ID for the item. This mutates the item so it needs to be
   * an object
   * @param {!Object} item The item to get the unique ID for.
   * @return {number} The unique ID for the item.
   */
  function getUid(item) {
    if (item.hasOwnProperty('uid'))
      return item.uid;
    return item.uid = createUid();
  }

  /**
   * Partially applies this function to a particular 'this object' and zero or
   * more arguments. The result is a new function with some arguments of the
   * first function pre-filled and the value of |this| 'pre-specified'.
   *
   * Remaining arguments specified at call-time are appended to the pre-
   * specified ones.
   *
   * Usage:
   * <pre>var barMethBound = bind(myFunction, myObj, 'arg1', 'arg2');
   * barMethBound('arg3', 'arg4');</pre>
   *
   * @param {Function} fn A function to partially apply.
   * @param {Object|undefined} selfObj Specifies the object which |this| should
   *     point to when the function is run. If the value is null or undefined,
   *     it will default to the global object.
   * @param {...*} var_args Additional arguments that are partially
   *     applied to the function.
   *
   * @return {!Function} A partially-applied form of the function bind() was
   *     invoked as a method of.
   */
  function bind(fn, selfObj, var_args) {
    var boundArgs = Array.prototype.slice.call(arguments, 2);
    return function() {
      var args = Array.prototype.slice.call(arguments);
      args.unshift.apply(args, boundArgs);
      return fn.apply(selfObj, args);
    }
  }

  /**
   * Dispatches a simple event on an event target.
   * @param {!EventTarget} target The event target to dispatch the event on.
   * @param {string} type The type of the event.
   * @param {boolean=} opt_bubbles Whether the event bubbles or not.
   * @param {boolean=} opt_cancelable Whether the default action of the event
   *     can be prevented.
   * @return {boolean} If any of the listeners called {@code preventDefault}
   *     during the dispatch this will return false.
   */
  function dispatchSimpleEvent(target, type, opt_bubbles, opt_cancelable) {
    var e = new cr.Event(type, opt_bubbles, opt_cancelable);
    return target.dispatchEvent(e);
  }

  /**
   * @param {string} name
   * @param {!Function} fun
   */
  function define(name, fun) {
    var obj = exportPath(name);
    var exports = fun();
    for (var key in exports) {
      obj[key] = exports[key];
    }
  }

  /**
   * Document used for various document related operations.
   * @type {!Document}
   */
  var doc = document;


  /**
   * Allows you to run func in the context of a different document.
   * @param {!Document} document The document to use.
   * @param {function():*} func The function to call.
   */
  function withDoc(document, func) {
    var oldDoc = doc;
    doc = document;
    try {
      func();
    } finally {
      doc = oldDoc;
    }
  }

  return {
    isMac: isMac,
    define: define,
    defineProperty: defineProperty,
    PropertyKind: PropertyKind,
    createUid: createUid,
    getUid: getUid,
    bind: bind,
    dispatchSimpleEvent: dispatchSimpleEvent,
    dispatchPropertyChange: dispatchPropertyChange,

    /**
     * The document that we are currently using.
     * @type {!Document}
     */
    get doc() {
      return doc;
    },
    withDoc: withDoc
  };
})();
