// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides common methods that can be shared by other JavaScripts.

goog.provide('__crWeb.common');

goog.require('__crWeb.base');


/** @typedef {HTMLInputElement|HTMLTextAreaElement|HTMLSelectElement} */
var FormControlElement;

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected. String 'common' is used in |__gCrWeb['common']| as it needs to be
 * accessed in Objective-C code.
 */
__gCrWeb.common = {};

// Store common namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['common'] = __gCrWeb.common;

/* Beginning of anonymous object. */
(function() {
  /**
   * JSON safe object to protect against custom implementation of Object.toJSON
   * in host pages.
   * @constructor
   */
  __gCrWeb.common.JSONSafeObject = function JSONSafeObject() {
  };

  /**
   * Protect against custom implementation of Object.toJSON in host pages.
   */
  __gCrWeb.common.JSONSafeObject.prototype['toJSON'] = null;

  /**
   * Retain the original JSON.stringify method where possible to reduce the
   * impact of sites overriding it
   */
  __gCrWeb.common.JSONStringify = JSON.stringify;

  /**
   * Prefix used in references to form elements that have no 'id' or 'name'
   */
  __gCrWeb.common.kNamelessFormIDPrefix = 'gChrome~';

  /**
   * Tests an element's visiblity. This test is expensive so should be used
   * sparingly.
   * @param {Element} element A DOM element.
   * @return {boolean} true if the |element| is currently part of the visible
   * DOM.
   */
  __gCrWeb.common.isElementVisible = function(element) {
    /** @type {Node} */
    var node = element;
    while (node && node !== document) {
      if (node.nodeType === Node.ELEMENT_NODE) {
        var style = window.getComputedStyle(/** @type {Element} */(node));
        if (style.display === 'none' || style.visibility === 'hidden') {
          return false;
        }
      }
      // Move up the tree and test again.
      node = node.parentNode;
    }
    // Test reached the top of the DOM without finding a concealed ancestor.
    return true;
  };

  /**
   * Based on Element::isFormControlElement() (WebKit)
   * @param {Element} element A DOM element.
   * @return {boolean} true if the |element| is a form control element.
   */
  __gCrWeb.common.isFormControlElement = function(element) {
    var tagName = element.tagName;
    return (tagName === 'INPUT' ||
            tagName === 'SELECT' ||
            tagName === 'TEXTAREA');
  };

  /**
   * Returns an array of control elements in a form.
   *
   * This method is based on the logic in method
   *     void WebFormElement::getFormControlElements(
   *         WebVector<WebFormControlElement>&) const
   * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
   * WebFormElement.cpp.
   *
   * @param {Element} form A form element for which the control elements are
   *   returned.
   * @return {Array<FormControlElement>}
   */
  __gCrWeb.common.getFormControlElements = function(form) {
    if (!form) {
      return [];
    }
    var results = [];
    // Get input and select elements from form.elements.
    // TODO(chenyu): according to
    // http://www.w3.org/TR/2011/WD-html5-20110525/forms.html, form.elements are
    // the "listed elements whose form owner is the form element, with the
    // exception of input elements whose type attribute is in the Image Button
    // state, which must, for historical reasons, be excluded from this
    // particular collection." In WebFormElement.cpp, this method is implemented
    // by returning elements in form's associated elements that have tag 'INPUT'
    // or 'SELECT'. Check if input Image Buttons are excluded in that
    // implementation. Note for Autofill, as input Image Button is not
    // considered as autofillable elements, there is no impact on Autofill
    // feature.
    var elements = form.elements;
    for (var i = 0; i < elements.length; i++) {
      if (__gCrWeb.common.isFormControlElement(elements[i])) {
        results.push(/** @type {FormControlElement} */ (elements[i]));
      }
    }
    return results;
  };

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
  __gCrWeb.common.autoComplete = function(element) {
    if (!element) {
      return false;
    }
    if (__gCrWeb.common.getLowerCaseAttribute(
        element, 'autocomplete') == 'off') {
      return false;
    }
    if (__gCrWeb.common.getLowerCaseAttribute(
            element.form, 'autocomplete') == 'off') {
      return false;
    }
    return true;
  };

  /**
   * Returns if an element is a text field.
   * This returns true for all of textfield-looking types such as text,
   * password, search, email, url, and number.
   *
   * This method aims to provide the same logic as method
   *     bool WebInputElement::isTextField() const
   * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
   * WebInputElement.cpp, where this information is from
   *     bool HTMLInputElement::isTextField() const
   *     {
   *       return m_inputType->isTextField();
   *     }
   * (chromium/src/third_party/WebKit/Source/WebCore/html/HTMLInputElement.cpp)
   *
   * The implementation here is based on the following:
   *
   * - Method bool InputType::isTextField() defaults to be false and it is
   *   override to return true only in HTMLInputElement's subclass
   *   TextFieldInputType (chromium/src/third_party/WebKit/Source/WebCore/html/
   *   TextFieldInputType.h).
   *
   * - The implementation here considers all the subclasses of
   *   TextFieldInputType: NumberInputType and BaseTextInputType, which has
   *   subclasses EmailInputType, PasswordInputType, SearchInputType,
   *   TelephoneInputType, TextInputType, URLInputType. (All these classes are
   *   defined in chromium/src/third_party/WebKit/Source/WebCore/html/)
   *
   * @param {Element} element An element to examine if it is a text field.
   * @return {boolean} true if element has type=text.
   */
  __gCrWeb.common.isTextField = function(element) {
    if (!element) {
      return false;
    }
    if (element.type === 'hidden') {
      return false;
    }
    return element.type === 'text' ||
           element.type === 'email' ||
           element.type === 'password' ||
           element.type === 'search' ||
           element.type === 'tel' ||
           element.type === 'url' ||
           element.type === 'number';
  };

  /**
   * Sets the checked value of an input and dispatches an change event if
   * |shouldSendChangeEvent|.
   *
   * This is a simplified version of the implementation of
   *
   *     void setChecked(bool nowChecked, TextFieldEventBehavior eventBehavior)
   *
   * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
   * WebInputElement.cpp, which calls
   *     void HTMLInputElement::setChecked(
   *         bool nowChecked, TextFieldEventBehavior eventBehavior)
   * in chromium/src/third_party/WebKit/Source/core/html/HTMLInputElement.cpp.
   *
   * @param {boolean} nowChecked The new checked value of the input element.
   * @param {Element} input The input element of which the value is set.
   * @param {boolean} shouldSendChangeEvent Whether a change event should be
   *     dispatched.
   */
  __gCrWeb.common.setInputElementChecked = function(
      nowChecked, input, shouldSendChangeEvent) {
    var checkedChanged = input.checked !== nowChecked;
    input.checked = nowChecked;
    if (checkedChanged) {
      __gCrWeb.common.createAndDispatchHTMLEvent(input, 'change', true, false);
    }
  };

  /**
   * Sets the value of an input and dispatches an change event if
   * |shouldSendChangeEvent|.
   *
   * It is based on the logic in
   *
   *     void setValue(const WebString&, bool sendChangeEvent = false)
   *
   * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
   * WebInputElement.cpp, which calls
   *    void setValue(const String& value, TextFieldEventBehavior eventBehavior)
   * in chromium/src/third_party/WebKit/Source/core/html/HTMLInputElement.cpp.
   *
   * @param {string} value The value the input element will be set.
   * @param {Element} input The input element of which the value is set.
   * @param {boolean} shouldSendChangeEvent Whether a change event should be
   *     dispatched.
   */
  __gCrWeb.common.setInputElementValue = function(
      value, input, shouldSendChangeEvent) {
    // In HTMLInputElement.cpp there is a check on canSetValue(value), which
    // returns false only for file input. As file input is not relevant for
    // autofill and this method is only used for autofill for now, there is no
    // such check in this implementation.
    var sanitizedValue = __gCrWeb.common.sanitizeValueForInputElement(
        value, input);
    var valueChanged = sanitizedValue !== input.value;
    input.value = sanitizedValue;
    if (valueChanged) {
      __gCrWeb.common.createAndDispatchHTMLEvent(input, 'change', true, false);
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
   __gCrWeb.common.sanitizeValueForInputElement = function(
       proposedValue, element) {
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
      return __gCrWeb.common.sanitizeValueForTextFieldInputType(
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
  __gCrWeb.common.sanitizeValueForTextFieldInputType = function(
      proposedValue, element) {
    var textFieldElementType = element.type;
    if (textFieldElementType === 'email') {
      return __gCrWeb.common.sanitizeValueForEmailInputType(
          proposedValue, element);
    } else if (textFieldElementType === 'number') {
      return __gCrWeb.common.sanitizeValueForNumberInputType(proposedValue);
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
  __gCrWeb.common.sanitizeValueForEmailInputType = function(
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
  __gCrWeb.common.sanitizeValueForNumberInputType = function(proposedValue) {
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
  __gCrWeb.common.trim = function(str) {
    return str.replace(/^\s+|\s+$/g, '');
  };

  /**
   * Returns the name that should be used for the specified |element| when
   * storing Autofill data. Various attributes are used to attempt to identify
   * the element, beginning with 'name' and 'id' attributes. Providing a
   * uniquely reversible identifier for any element is a non-trivial problem;
   * this solution attempts to satisfy the majority of cases.
   *
   * It aims to provide the logic in
   *     WebString nameForAutofill() const;
   * in chromium/src/third_party/WebKit/Source/WebKit/chromium/public/
   *  WebFormControlElement.h
   *
   * @param {Element} element An element of which the name for Autofill will be
   *     returned.
   * @return {string} the name for Autofill.
   */
  __gCrWeb.common.nameForAutofill = function(element) {
    if (!element) {
      return '';
    }
    var trimmedName = element.name;
    if (trimmedName) {
      trimmedName = __gCrWeb.common.trim(trimmedName);
      if (trimmedName.length > 0) {
        return trimmedName;
      }
    }
    trimmedName = element.getAttribute('id');
    if (trimmedName) {
      return __gCrWeb.common.trim(trimmedName);
    }
    return '';
  };

  /**
   * Acquires the specified DOM |attribute| from the DOM |element| and returns
   * its lower-case value, or null if not present.
   * @param {Element} element A DOM element.
   * @param {string} attribute An attribute name.
   * @return {?string} Lowercase value of DOM element or null if not present.
   */
  __gCrWeb.common.getLowerCaseAttribute = function(element, attribute) {
    if (!element) {
      return null;
    }
    var value = element.getAttribute(attribute);
    if (value) {
      return value.toLowerCase();
    }
    return null;
  };

  /**
   * Converts a relative URL into an absolute URL.
   * @param {Object} doc Document.
   * @param {string} relativeURL Relative URL.
   * @return {string} Absolute URL.
   */
  __gCrWeb.common.absoluteURL = function(doc, relativeURL) {
    // In the case of data: URL-based pages, relativeURL === absoluteURL.
    if (doc.location.protocol === 'data:') {
      return doc.location.href;
    }
    var urlNormalizer = doc['__gCrWebURLNormalizer'];
    if (!urlNormalizer) {
      urlNormalizer = doc.createElement('a');
      doc['__gCrWebURLNormalizer'] = urlNormalizer;
    }

    // Use the magical quality of the <a> element. It automatically converts
    // relative URLs into absolute ones.
    urlNormalizer.href = relativeURL;
    return urlNormalizer.href;
  };

  /**
   * Extracts the webpage URL from the given URL by removing the query
   * and the reference (aka fragment) from the URL.
   * @param {string} url Web page URL.
   * @return {string} Web page URL with query and reference removed.
   */
  __gCrWeb.common.removeQueryAndReferenceFromURL = function(url) {
    var queryIndex = url.indexOf('?');
    if (queryIndex != -1) {
      return url.substring(0, queryIndex);
    }

    var hashIndex = url.indexOf('#');
    if (hashIndex != -1) {
      return url.substring(0, hashIndex);
    }
    return url;
  };

  /**
   * Returns the form's |name| attribute if non-empty; otherwise the form's |id|
   * attribute, or the index of the form (with prefix) in document.forms.
   *
   * It is partially based on the logic in
   *     const string16 GetFormIdentifier(const blink::WebFormElement& form)
   * in chromium/src/components/autofill/renderer/form_autofill_util.h.
   *
   * @param {Element} form An element for which the identifier is returned.
   * @return {string} a string that represents the element's identifier.
   */
  __gCrWeb.common.getFormIdentifier = function(form) {
    if (!form)
      return '';
    var name = form.getAttribute('name');
    if (name && name.length != 0) {
      return name;
    }
    name = form.getAttribute('id');
    if (name) {
      return name;
    }
    // A form name must be supplied, because the element will later need to be
    // identified from the name. A last resort is to take the index number of
    // the form in document.forms. ids are not supposed to begin with digits (by
    // HTML 4 spec) so this is unlikely to match a true id.
    for (var idx = 0; idx != document.forms.length; idx++) {
      if (document.forms[idx] == form) {
        return __gCrWeb.common.kNamelessFormIDPrefix + idx;
      }
    }
    return '';
  };

  /**
   * Returns the form element from an ID obtained from getFormIdentifier.
   *
   * This works on a 'best effort' basis since DOM changes can always change the
   * actual element that the ID refers to.
   *
   * @param {string} name An ID string obtained via getFormIdentifier.
   * @return {Element} The original form element, if it can be determined.
   */
  __gCrWeb.common.getFormElementFromIdentifier = function(name) {
    // First attempt is from the name / id supplied.
    var form = document.forms.namedItem(name);
    if (form) {
      if (form.nodeType !== Node.ELEMENT_NODE)
        return null;
      return /** @type {Element} */(form);
    }
    // Second attempt is from the prefixed index position of the form in
    // document.forms.
    if (name.indexOf(__gCrWeb.common.kNamelessFormIDPrefix) == 0) {
      var nameAsInteger = 0 |
          name.substring(__gCrWeb.common.kNamelessFormIDPrefix.length);
      if (__gCrWeb.common.kNamelessFormIDPrefix + nameAsInteger == name &&
          nameAsInteger < document.forms.length) {
        return document.forms[nameAsInteger];
      }
    }
    return null;
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
  __gCrWeb.common.createAndDispatchHTMLEvent = function(
      element, type, bubbles, cancelable) {
    var changeEvent =
        /** @type {!Event} */(element.ownerDocument.createEvent('HTMLEvents'));
    changeEvent.initEvent(type, bubbles, cancelable);

    // A timer is used to avoid reentering JavaScript evaluation.
    window.setTimeout(function() {
      element.dispatchEvent(changeEvent);
    }, 0);
  };

  /**
   * Retrieves favicon information.
   *
   * @return {Object} Object containing favicon data.
   */
  __gCrWeb.common.getFavicons = function() {
    var favicons = [];
    var hasFavicon = false;
    delete favicons.toJSON;  // Never inherit Array.prototype.toJSON.
    var links = document.getElementsByTagName('link');
    var linkCount = links.length;
    for (var i = 0; i < linkCount; ++i) {
      if (links[i].rel) {
        var rel = links[i].rel.toLowerCase();
        if (rel == 'shortcut icon' ||
            rel == 'icon' ||
            rel == 'apple-touch-icon' ||
            rel == 'apple-touch-icon-precomposed') {
          var favicon = {
            rel: links[i].rel.toLowerCase(),
            href: links[i].href
          };
          favicons.push(favicon);
          if (rel == 'icon' || rel == 'shortcut icon') {
            hasFavicon = true;
          }
        }
      }
    }
    if (!hasFavicon) {
      // If an HTTP(S)? webpage does not reference a "favicon" then search
      // for a file named "favicon.ico" at the root of the website (legacy).
      // http://en.wikipedia.org/wiki/Favicon
      var location = document.location;
      if (location.protocol == 'http:' || location.protocol == 'https:') {
        var favicon = {
          rel: 'icon',
          href: location.origin + '/favicon.ico'
        };
        favicons.push(favicon);
      }
    }
    return favicons;
  };

  /**
   * Checks whether an <object> node is plugin content (as <object> can also be
   * used to embed images).
   * @param {HTMLElement} node The <object> node to check.
   * @return {boolean} Whether the node appears to be a plugin.
   * @private
   */
  var objectNodeIsPlugin_ = function(node) {
    return node.hasAttribute('classid') ||
           (node.hasAttribute('type') && node.type.indexOf('image/') != 0);
  };

  /**
   * Checks whether plugin a node has fallback content.
   * @param {HTMLElement} node The node to check.
   * @return {boolean} Whether the node has fallback.
   * @private
   */
  var pluginHasFallbackContent_ = function(node) {
    return node.textContent.trim().length > 0 ||
           node.getElementsByTagName('img').length > 0;
  };

  /**
   * Returns a list of plugin elements in the document that have no fallback
   * content. For nested plugins, only the innermost plugin element is returned.
   * @return {!Array<!HTMLElement>} A list of plugin elements.
   * @private
   */
  var findPluginNodesWithoutFallback_ = function() {
    var i, pluginNodes = [];
    var objects = document.getElementsByTagName('object');
    var objectCount = objects.length;
    for (i = 0; i < objectCount; i++) {
      var object = /** @type {!HTMLElement} */(objects[i]);
      if (objectNodeIsPlugin_(object) &&
          !pluginHasFallbackContent_(object)) {
        pluginNodes.push(object);
      }
    }
    var applets = document.getElementsByTagName('applet');
    var appletsCount = applets.length;
    for (i = 0; i < appletsCount; i++) {
      var applet = /** @type {!HTMLElement} */(applets[i]);
      if (!pluginHasFallbackContent_(applet)) {
        pluginNodes.push(applet);
      }
    }
    return pluginNodes;
  };

  /**
   * Finds and stores any plugins that don't have placeholders.
   * Returns true if any plugins without placeholders are found.
   */
  __gCrWeb.common.updatePluginPlaceholders = function() {
    var plugins = findPluginNodesWithoutFallback_();
    if (plugins.length > 0) {
      // Store the list of plugins in a known place for the replacement script
      // to use, then trigger it.
      __gCrWeb['placeholderTargetPlugins'] = plugins;
      return true;
    }
    return false;
  };
}());  // End of anonymous object
