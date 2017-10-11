// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. See http://goo.gl/FwOgy
//
// Installs password management functions on the |__gCrWeb| object.
//
// Finds all password forms in the current document and extracts
// their attributes and elements using the same logic as
// third_party/WebKit/Source/WebCore/html/HTMLFormElement.cpp
//
// Returns a JSON string representing an array of objects,
// where each object represents a password form with the discovered
// elements and their values.
//
// The search for password form fields follows the same algorithm
// as the WebKit implementation, see http://goo.gl/4hwh6

// Only install the password management functions once.
if (__gCrWeb && !__gCrWeb['fillPasswordForm']) {

  /**
   * Finds all password forms in the window and returns form data as a JSON
   * string.
   * @return {string} Form data as a JSON string.
   */
  __gCrWeb['findPasswordForms'] = function() {
    var formDataList = [];
    if (hasPasswordField_(window)) {
      __gCrWeb.getPasswordFormDataList(formDataList, window);
    }
    return __gCrWeb.stringify(formDataList);
  };

  /** Returns true if the supplied window or any frames inside contain an input
   * field of type 'password'.
   * @private
   */
  var hasPasswordField_ = function(win) {
    var doc = win.document;

    // We may will not be allowed to read the 'document' property from a frame
    // that is in a different domain.
    if (!doc) {
      return false;
    }

    if (doc.querySelector('input[type=password]')) {
      return true;
    }

    var frames = getSameOriginFrames_(win);
    for (var i = 0; i < frames.length; i++) {
      if (hasPasswordField_(frames[i])) {
        return true;
      }
    }

    return false;
  };

  /**
   * Returns the contentWindow of all iframes that are from the the same origin
   * as the containing window.
   * @param {Window} win The window in which to look for frames.
   * @return {Array.<Window>} Array of the same-origin frames found.
   */
  var getSameOriginFrames_ = function(win) {
    var frames = win.document.getElementsByTagName("iframe");
    var result = [];
    for (var i = 0; i < frames.length; i++) {
      if (!frames[i].src ||
          __gCrWeb.common.isSameOrigin(win.location.href, frames[i].src)) {
        result.push(frames[i].contentWindow);
      }
    }
    return result;
  };

  /**
   * Returns a canonical action for |formElement|. It works the same as upstream
   * function GetCanonicalActionForForm.
   * @param {Form} formElement.
   * @return {string} Canonical action.
   */
  var getCanonicalActionForForm_ = function(formElement) {
    var raw_action = formElement.getAttribute('action') || "";
    var absolute_url = __gCrWeb.common.absoluteURL(
          formElement.ownerDocument, raw_action);
    return __gCrWeb.common.removeQueryAndReferenceFromURL(absolute_url);
  };

  /**
   * If |form| has no submit elements and exactly 1 button that button
   * is assumed to be a submit button. This function adds onSubmitButtonClick_
   * as a handler for touchend event of this button. Touchend event is used as
   * a proxy for onclick event because onclick handling might be prevented by
   * the site JavaScript.
   */
  var addSubmitButtonTouchEndHandler_ = function(form) {
    if (form.querySelector('input[type=submit]'))
      return;
    var buttons = form.querySelectorAll('button');
    if (buttons.length != 1)
      return;
    buttons[0].addEventListener('touchend', onSubmitButtonTouchEnd_);
   };

   /**
    * Click handler for the submit button. It sends to the host
    * form.submitButtonClick command.
    */
   var onSubmitButtonTouchEnd_ = function(evt) {
       var form = evt.currentTarget.form;
       var formData = __gCrWeb.getPasswordFormData(form);
       if (!formData)
         return;
       formData["command"] = 'passwordForm.submitButtonClick';
       __gCrWeb.message.invokeOnHost(formData);
    };

  /**
   * Returns the password form with the given |name| as a JSON string.
   * @param {string} name The name of the form to extract.
   * @return {string} The password form.
   */
  __gCrWeb['getPasswordForm'] = function(name) {
    var el = __gCrWeb.common.getFormElementFromIdentifier(name);
    if (!el)
      return 'noPasswordsFound';
    var formData = __gCrWeb.getPasswordFormData(el);
    if (!formData)
      return 'noPasswordsFound';
    return __gCrWeb.stringify(formData);
  };

  /**
   * Returns an array of forms on the page that match the structure described by
   * |formData|. The form matching logic follows that in
   * chrome/renderer/autofill/password_autofill_manager.h.
   * @param {Object} formData Form data.
   * @param {Object} doc A document containing formData.
   * @param {string=} opt_normalizedAction The action URL to compare to.
   * @return {Array.<Element>} Array of forms found.
   */
  __gCrWeb.findMatchingPasswordForms = function(formData, doc,
                                                opt_normalizedAction) {
    var forms = doc.forms;
    var fields = formData['fields'];
    var matching = [];
    for (var i = 0; i < forms.length; i++) {
      var form = forms[i];
      var normalizedFormAction = opt_normalizedAction ||
          getCanonicalActionForForm_(form);
      if (formData.action != normalizedFormAction) {
        continue;
      }

      // We need to find all input fields matching |formData| in this form,
      // otherwise it is the wrong form.
      var inputs = form.getElementsByTagName('input');
      var foundAllFields = true;
      for (var fieldIndex = 0; fieldIndex < fields.length; fieldIndex++) {
        var name = fields[fieldIndex]['name'];
        var value = fields[fieldIndex]['value'];
        // The first field in |formData| is always the username field,
        // the second is always the password field.
        var findingUsername = fieldIndex == 0;
        var findingPassword = fieldIndex == 1;
        var foundField = false;
        for (var k = 0; k < inputs.length; k++) {
          var input = inputs[k];

          // Ensure that the field is the right type.
          if (findingPassword && input.type != 'password') {
            continue;
          }
          if (!findingPassword && (input.type == 'password' ||
                                   !__gCrWeb.common.isTextField(input))) {
            continue;
          }

          // Skip read-only fields without a value since they cannot be filled.
          if (input.readOnly && input.value == '') {
            continue;
          }

          // If more than one match is made, then we have an ambiguity (due to
          // misuse of 'name' attribute) and the form is considered a mismatch.
          if (input.name == name) {
            if (foundField) {
              foundField = false;
              break;
            }
            foundField = true;
          }
        }

        if (!foundField) {
          foundAllFields = false;
          break;
        }
      }

      if (foundAllFields) {
        matching.push(form);
      }
    }
    return matching;
  };

  /**
   * Clears autofilled credentials in the form with the specified name.
   * @param {string} formName The name of the form to clear.
   * @return {boolean} Whether the form was successfully cleared.
   */
  __gCrWeb['clearAutofilledPasswords'] = function(formName) {
    var el = __gCrWeb.common.getFormElementFromIdentifier(formName);
    if (!el)
      return false;
    var formData = __gCrWeb.getPasswordFormData(el);
    if (!formData)
      return false;
    var usernameElement =
        __gCrWeb.getElementByNameWithParent(el, formData.usernameElement);
    __gCrWeb.setAutofilled(usernameElement, false);
    formData.passwords.forEach(function(password) {
      var passwordElement =
          __gCrWeb.getElementByNameWithParent(el, password.element, true);
      if (__gCrWeb.isAutofilled(passwordElement)) {
        __gCrWeb.setAutofilled(passwordElement, false);
        passwordElement.value = '';
      }
    });
    return true;
  };

  /**
   * Finds the form described by |formData| and fills in the
   * username and password values.
   *
   * This is a public function invoked by Chrome. There is no information
   * passed to this function that the page does not have access to anyway.
   *
   * @param {!Object.<string, *>} formData Dictionary of parameters,
   *    including:
   *      'action': <string> The form action URL;
   *      'fields': {Array.{Object.<string, string>}} Field name/value pairs;
   * @param {string} username The username to fill.
   * @param {string} password The password to fill.
   * @param {string=} opt_normalizedOrigin The origin URL to compare to.
   * @return {boolean} Whether a form field has been filled.
   */
  __gCrWeb['fillPasswordForm'] = function(formData, username, password,
                                          opt_normalizedOrigin) {
  var normalizedOrigin = opt_normalizedOrigin ||
      __gCrWeb.common.removeQueryAndReferenceFromURL(window.location.href);
  var origin = formData['origin'];
  if (!__gCrWeb.common.isSameOrigin(origin, normalizedOrigin)) {
    return false;
  }
  return fillPasswordFormWithData_(
        formData, username, password, window, opt_normalizedOrigin);
  };

  /**
   * Returns the element with the specified name that is a child of the
   * specified parent element.
   * @param {Element} parent The parent of the desired element.
   * @param {string} name The name of the desired element.
   * @param {boolen} isPassword Whether the field should be a password field;
   *     if not supplied, |false| is assumed.
   * @return {Element} The element if found, otherwise null;
   */
  __gCrWeb['getElementByNameWithParent'] = function(
      parent, name, isPassword) {
    isPassword = isPassword || false;
    var parentType = parent.type || "";
    var isParentPassword = parentType === "password";
    if (parent.name === name && (isPassword === isParentPassword)) {
      return parent;
    }
    for (var i = 0; i < parent.children.length; i++) {
      var el = __gCrWeb.getElementByNameWithParent(
          parent.children[i], name, isPassword);
      if (el) {
        return el;
      }
    }
    return null;
  };

  /**
   * Given a description of a form (origin, action and input fields),
   * finds that form on the page and fills in the specified username
   * and password.
   *
   * @param {Object} formData Form data.
   * @param {string} username The username to fill.
   * @param {string} password The password to fill.
   * @param {Object} win A window or a frame containing formData.
   * @param {string=} opt_normalizedOrigin The origin URL to compare to.
   * @return {boolean} Whether a form field has been filled.
   */
  var fillPasswordFormWithData_ = function(
      formData, username, password, win, opt_normalizedOrigin) {
    var doc = win.document;
    var filled = false;

    __gCrWeb.findMatchingPasswordForms(formData, doc, opt_normalizedOrigin).
        forEach(function(form) {
      var usernameInput =
          __gCrWeb.getElementByNameWithParent(form, formData.fields[0].name);
      var passwordInput =
          __gCrWeb.getElementByNameWithParent(
              form, formData.fields[1].name, true);
      if (!usernameInput.disabled && !passwordInput.disabled) {
        // If username was provided on a read-only field and it matches the
        // requested username, fill the form.
        if (usernameInput.readOnly && usernameInput.value) {
          if (usernameInput.value == username) {
            passwordInput.value = password;
            __gCrWeb.setAutofilled(passwordInput, true);
            filled = true;
          }
        } else {
          // Setting input fields via .value assignment does not trigger all
          // the events that a web site can observe. This has the effect of
          // certain web sites rejecting an autofilled sign in form as not
          // signed in because the user didn't actually "typed" into the field.
          // Adding the .focus() works around this problems.
          usernameInput.focus();
          usernameInput.value = username;
          passwordInput.focus();
          passwordInput.value = password;
          __gCrWeb.setAutofilled(passwordInput, true);
          __gCrWeb.setAutofilled(usernameInput, true);
          filled = true;
        }
      }
    });

    // Recursively invoke for all iframes.
    var frames = getSameOriginFrames_(win);
    for (var i = 0; i < frames.length; i++) {
      if (fillPasswordFormWithData_(
          formData, username, password, frames[i], opt_normalizedOrigin)) {
        filled = true;
      }
    }

    return filled;
  };

  /**
   * Returns true if the supplied field |inputElement| was autofilled.
   * @param {Element} inputElement The form field for which we need to
   *     acquire the autofilled indicator.
   * @return {boolean} Whether inputElement was autofilled.
   */
  __gCrWeb.isAutofilled = function(inputElement) {
    return inputElement['__gCrWebAutofilled'];
  };

  /**
   * Marks the supplied field as autofilled or not depending on the
   * |value|.
   * @param {Element} inputElement The form field for which the indicator
   *    needs to be set.
   * @param {boolean} value The new value of the indicator.
   */
  __gCrWeb.setAutofilled = function(inputElement, value) {
    inputElement['__gCrWebAutofilled'] = value;
  };

  /**
   * Selects text starting from |selectFrom| in the specified field.
   * @param {string} formName The name of the form to select in.
   * @param {string} fieldName The name of the field to select in.
   * @param {number} selectFrom The starting index for selection.
   * @return {boolean} Whether the operation was successful.
   */
  __gCrWeb['selectText'] = function(formName, fieldName, selectFrom) {
    var form = __gCrWeb.common.getFormElementFromIdentifier(formName);
    var el = __gCrWeb.getElementByNameWithParent(form, fieldName);
    if (!el)
      return false;
    el.selectionStart = selectFrom;
    el.selectionEnd = el.value.length;
    return true;
  };

  /**
   * Fills all password fields in the form identified by |formName|
   * with |password| and marks them as autofilled.
   *
   * @param {string} formName The name of the form to fill.
   * @param {string} password The password to fill.
   * @return {boolean} Whether a password field has been filled.
   */
  __gCrWeb['fillPasswordFormWithGeneratedPassword'] =
      function(formName, password) {
    var form = __gCrWeb.common.getFormElementFromIdentifier(formName);
    if (!form)
      return false;
    var fields = form.querySelectorAll('input[type=password]');
    for (var i = 0; i < fields.length; i++) {
      var field = fields[i];
      field.value = password;
      __gCrWeb.setAutofilled(field, true);
    }
    return fields.length > 0;
  };

  /**
   * Finds all forms with passwords in the supplied window or frame and appends
   * JS objects containing the form data to |formDataList|.
   * @param {!Array.<Object>} formDataList A list that this function populates
   *     with descriptions of discovered forms.
   * @param {Window} win A window (or frame) in which the function should
   *    look for password forms.
   */
  __gCrWeb.getPasswordFormDataList = function(formDataList, win) {
    var doc = null;

    try {
      // Security violations may generate an exception or null to be returned.
      doc = win.document;
    } catch(e) {
    }

    if (!doc) {
      return;
    }

    var forms = doc.forms;
    for (var i = 0; i < forms.length; i++) {
      var formData = __gCrWeb.getPasswordFormData(forms[i]);
      if (formData) {
        formDataList.push(formData);
        addSubmitButtonTouchEndHandler_(forms[i]);
      }
    }

    // Recursively invoke for all iframes.
    var frames = getSameOriginFrames_(win);
    for (var i = 0; i < frames.length; i++) {
      __gCrWeb.getPasswordFormDataList(formDataList, frames[i]);
    }
  };

  /**
   * Returns a JS object containing the data from |formElement|.
   * @param {Element} formElement An HTML Form element.
   * @return {Object} Object of data from formElement.
   */
  __gCrWeb.getPasswordFormData = function(formElement) {
    var inputs = formElement.getElementsByTagName('input');

    var fields = [];
    var passwords = [];
    var firstPasswordIndex = 0;
    for (var j = 0; j < inputs.length; j++) {
      // TODO(dplotnikov): figure out a way to identify the activated
      // submit, which is the button that the user has already hit
      // before this code is called.

      var input = inputs[j];

      fields.push({
        'element': input.name,
        'type': input.type
      });

      if (!input.disabled && input.type == 'password') {
        if (passwords.length == 0) {
          firstPasswordIndex = j;
        }
        passwords.push({
          'element': input.name,
          'value': input.value
        });
      }
    }

    if (passwords.length == 0)
      return null;

    var usernameElement = '';
    var usernameValue = '';
    for (var j = firstPasswordIndex - 1; j >= 0; j--) {
      var input = inputs[j];
      if (!input.disabled && __gCrWeb.common.isTextField(input)) {
        usernameElement = input.name;
        usernameValue = input.value;
        break;
      }
    }

    var origin = __gCrWeb.common.removeQueryAndReferenceFromURL(
        formElement.ownerDocument.location.href);

    return {
      'action': getCanonicalActionForForm_(formElement),
      'name': __gCrWeb.common.getFormIdentifier(formElement),
      'origin': origin,
      'fields': fields,
      'usernameElement': usernameElement,
      'usernameValue': usernameValue,
      'passwords': passwords
    };
  };
}
