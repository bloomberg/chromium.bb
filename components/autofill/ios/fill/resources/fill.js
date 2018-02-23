// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides methods used to fill forms in JavaScript.

goog.module('__crWeb.fill');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected. String 'fill' is used in |__gCrWeb['fill']| as it needs to be
 * accessed in Objective-C code.
 */
__gCrWeb.fill = {};

// Store fill namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['fill'] = __gCrWeb.fill;

/* Beginning of anonymous object. */
(function() {

/**
 * Returns true if an element can be autocompleted.
 *
 * This method aims to provide the same logic as method
 *     bool autoComplete() const
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebFormElement.cpp.
 *
 * @param {Element} element An element to check if it can be autocompleted.
 * @return {boolean} true if element can be autocompleted.
 */
__gCrWeb.fill.autoComplete = function(element) {
  if (!element) {
    return false;
  }
  if (__gCrWeb.common.getLowerCaseAttribute(element, 'autocomplete') == 'off') {
    return false;
  }
  if (__gCrWeb.common.getLowerCaseAttribute(element.form, 'autocomplete') ==
      'off') {
    return false;
  }
  return true;
};

/**
 * Sets the value of a data-bound input using AngularJS.
 *
 * The method first set the value using the val() method. Then, if input is
 * bound to a model value, it sets the model value.
 * Documentation of relevant modules of AngularJS can be found at
 * https://docs.angularjs.org/guide/databinding
 * https://docs.angularjs.org/api/auto/service/$injector
 * https://docs.angularjs.org/api/ng/service/$parse
 *
 * @param {string} value The value the input element will be set.
 * @param {Element} input The input element of which the value is set.
 **/
function setInputElementAngularValue_(value, input) {
  if (!input || !window['angular']) {
    return;
  }
  var angular_element =
      window['angular'].element && window['angular'].element(input);
  if (!angular_element) {
    return;
  }
  angular_element.val(value);
  var angular_model = angular_element.data && angular_element.data('ngModel');
  if (!angular_model) {
    return;
  }
  angular_element.injector().invoke([
    '$parse',
    function(parse) {
      var setter = parse(angular_model);
      setter.assign(angular_element.scope(), value);
    }
  ]);
}

/**
 * Sets the value of an input and dispatches a change event if
 * |shouldSendChangeEvent|.
 *
 * It is based on the logic in
 *
 *     void setValue(const WebString&, bool sendChangeEvent = false)
 *
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebInputElement.cpp, which calls
 *    void setValue(const String& value, TextFieldEventBehavior eventBehavior)
 * or
 *    void setChecked(bool nowChecked, TextFieldEventBehavior eventBehavior)
 * in chromium/src/third_party/WebKit/Source/core/html/HTMLInputElement.cpp.
 *
 * @param {string} value The value the input element will be set.
 * @param {Element} input The input element of which the value is set.
 * @param {boolean} shouldSendChangeEvent Whether a change event should be
 *     dispatched.
 */
__gCrWeb.fill.setInputElementValue = function(
    value, input, shouldSendChangeEvent) {
  if (!input) {
    return;
  }
  var changed = false;
  if (input.type === 'checkbox' || input.type === 'radio') {
    changed = input.checked !== value;
    input.checked = value;
  } else if (input.type === 'select-one') {
    changed = input.value !== value;
    input.value = value;
  } else {
    // In HTMLInputElement.cpp there is a check on canSetValue(value), which
    // returns false only for file input. As file input is not relevant for
    // autofill and this method is only used for autofill for now, there is no
    // such check in this implementation.
    var sanitizedValue =
        __gCrWeb.fill.sanitizeValueForInputElement(value, input);
    changed = sanitizedValue !== input.value;
    input.value = sanitizedValue;
  }
  if (window['angular']) {
    // The page uses the AngularJS framework. Update the angular value before
    // sending events.
    setInputElementAngularValue_(value, input);
  }
  if (changed && shouldSendChangeEvent) {
    __gCrWeb.fill.notifyElementValueChanged(input);
  }
};

/**
 * Returns a sanitized value of proposedValue for a given input element type.
 * The logic is based on
 *
 *      String sanitizeValue(const String&) const
 *
 * in chromium/src/third_party/WebKit/Source/core/html/InputType.h
 *
 * @param {string} proposedValue The proposed value.
 * @param {Element} element The element for which the proposedValue is to be
 *     sanitized.
 * @return {string} The sanitized value.
 */
__gCrWeb.fill.sanitizeValueForInputElement = function(proposedValue, element) {
  if (!proposedValue) {
    return '';
  }

  // Method HTMLInputElement::sanitizeValue() calls InputType::sanitizeValue()
  // (chromium/src/third_party/WebKit/Source/core/html/InputType.cpp) for
  // non-null proposedValue. InputType::sanitizeValue() returns the original
  // proposedValue by default and it is overridden in classes
  // BaseDateAndTimeInputType, ColorInputType, RangeInputType and
  // TextFieldInputType (all are in
  // chromium/src/third_party/WebKit/Source/core/html/). Currently only
  // TextFieldInputType is relevant and sanitizeValue() for other types of
  // input elements has not been implemented.
  if (__gCrWeb.common.isTextField(element)) {
    return __gCrWeb.fill.sanitizeValueForTextFieldInputType(
        proposedValue, element);
  }
  return proposedValue;
};

/**
 * Returns a sanitized value for a text field.
 *
 * The logic is based on |String sanitizeValue(const String&)|
 * in chromium/src/third_party/WebKit/Source/core/html/TextFieldInputType.h
 * Note this method is overridden in EmailInputType and NumberInputType.
 *
 * @param {string} proposedValue The proposed value.
 * @param {Element} element The element for which the proposedValue is to be
 *     sanitized.
 * @return {string} The sanitized value.
 */
__gCrWeb.fill.sanitizeValueForTextFieldInputType = function(
    proposedValue, element) {
  var textFieldElementType = element.type;
  if (textFieldElementType === 'email') {
    return __gCrWeb.fill.sanitizeValueForEmailInputType(proposedValue, element);
  } else if (textFieldElementType === 'number') {
    return __gCrWeb.fill.sanitizeValueForNumberInputType(proposedValue);
  }
  var valueWithLineBreakRemoved = proposedValue.replace(/(\r\n|\n|\r)/gm, '');
  // TODO(chenyu): Should we also implement numCharactersInGraphemeClusters()
  // in chromium/src/third_party/WebKit/Source/core/platform/text/
  // TextBreakIterator.cpp and call it here when computing newLength?
  // Different from the implementation in TextFieldInputType.h, where a limit
  // on the text length is considered due to
  // https://bugs.webkit.org/show_bug.cgi?id=14536, no such limit is
  // considered here for now.
  var newLength = valueWithLineBreakRemoved.length;
  // This logic is from method String limitLength() in TextFieldInputType.h
  for (var i = 0; i < newLength; ++i) {
    var current = valueWithLineBreakRemoved[i];
    if (current < ' ' && current != '\t') {
      newLength = i;
      break;
    }
  }
  return valueWithLineBreakRemoved.substring(0, newLength);
};

/**
 * Returns the sanitized value for an email input.
 *
 * The logic is based on
 *
 *     String EmailInputType::sanitizeValue(const String& proposedValue) const
 *
 * in chromium/src/third_party/WebKit/Source/core/html/EmailInputType.cpp
 *
 * @param {string} proposedValue The proposed value.
 * @param {Element} element The element for which the proposedValue is to be
 *     sanitized.
 * @return {string} The sanitized value.
 */
__gCrWeb.fill.sanitizeValueForEmailInputType = function(
    proposedValue, element) {
  var valueWithLineBreakRemoved = proposedValue.replace(/(\r\n|\n\r)/gm, '');

  if (!element.multiple) {
    return __gCrWeb.common.trim(proposedValue);
  }
  var addresses = valueWithLineBreakRemoved.split(',');
  for (var i = 0; i < addresses.length; ++i) {
    addresses[i] = __gCrWeb.common.trim(addresses[i]);
  }
  return addresses.join(',');
};

/**
 * Returns the sanitized value of a proposed value for a number input.
 *
 * The logic is based on
 *
 *     String NumberInputType::sanitizeValue(const String& proposedValue)
 *         const
 *
 * in chromium/src/third_party/WebKit/Source/core/html/NumberInputType.cpp
 *
 * Note in this implementation method Number() is used in the place of method
 * parseToDoubleForNumberType() called in NumberInputType.cpp.
 *
 * @param {string} proposedValue The proposed value.
 * @return {string} The sanitized value.
 */
__gCrWeb.fill.sanitizeValueForNumberInputType = function(proposedValue) {
  var sanitizedValue = Number(proposedValue);
  if (isNaN(sanitizedValue)) {
    return '';
  }
  return sanitizedValue.toString();
};

/**
 * Trims any whitespace from the start and end of a string.
 * Used in preference to String.prototype.trim as this can be overridden by
 * sites.
 *
 * @param {string} str The string to be trimmed.
 * @return {string} The string after trimming.
 */


/**
 * Creates and sends notification that element has changed.
 *
 * Most handlers react to 'change' or 'input' event, so sent both.
 *
 * @param {Element} element The element that changed.
 */
__gCrWeb.fill.notifyElementValueChanged = function(element) {
  __gCrWeb.fill.createAndDispatchHTMLEvent(element, 'keydown', true, false);
  __gCrWeb.fill.createAndDispatchHTMLEvent(element, 'change', true, false);
  __gCrWeb.fill.createAndDispatchHTMLEvent(element, 'input', true, false);
  __gCrWeb.fill.createAndDispatchHTMLEvent(element, 'keyup', true, false);
};

/**
 * Creates and dispatches an HTML event.
 *
 * @param {Element} element The element for which an event is created.
 * @param {string} type The type of the event.
 * @param {boolean} bubbles A boolean indicating whether the event should
 *     bubble up through the event chain or not.
 * @param {boolean} cancelable A boolean indicating whether the event can be
 *     canceled.
 */
__gCrWeb.fill.createAndDispatchHTMLEvent = function(
    element, type, bubbles, cancelable) {
  var changeEvent =
      /** @type {!Event} */ (element.ownerDocument.createEvent('HTMLEvents'));
  changeEvent.initEvent(type, bubbles, cancelable);
  // Some frameworks will use the data field to update their cache value.
  changeEvent.data = element.value;

  // Adding a |simulated| flag on the event will force the React framework to
  // update the backend store.
  changeEvent.simulated = true;

  element.dispatchEvent(changeEvent);
};


}());  // End of anonymous object
