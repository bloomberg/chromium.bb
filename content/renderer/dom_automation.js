// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// dom_automation.js
// Methods for performing common DOM operations. Used in Chrome testing
// involving the DomAutomationController.

var domAutomation = domAutomation || {};

(function() {
  // |objects| is used to track objects which are sent back to the
  // DomAutomationController. Since JavaScript does not have a map type,
  // |objects| is simply an object in which the property name and
  // property value serve as the key-value pair. The key is the handle number
  // and the value is the tracked object.
  domAutomation.objects = {};

  // The next object handle to use.
  domAutomation.nextHandle = 1;

  // The current call ID for which a response is awaited. Each asynchronous
  // function is given a call ID. When the function has a result to return,
  // it must supply that call ID. If a result has not yet been received for
  // that call ID, a response containing the result will be sent to the
  // domAutomationController.
  domAutomation.currentCallId = 1;

  // The current timeout for an asynchronous JavaScript evaluation. Can be given
  // to window.clearTimeout.
  domAutomation.currentTimeout = null;

  // Returns |value| after converting it to an acceptable type for return, if
  // necessary.
  function getConvertedValue(value) {
    if (typeof value == "undefined" || !value) {
      return "";
    }
    if (value instanceof Array) {
      var result = [];
      for (var i = 0; i < value.length; i++) {
        result.push(getConvertedValue(value[i]));
      }
      return result;
    }
    if (typeof(value) == "object") {
      var handle = getHandleForObject(value);
      if (handle == -1) {
        // Track this object.
        var handle = domAutomation.nextHandle++;
        domAutomation.objects[handle] = value;
      }
      return handle;
    }
    return value;
  }

  // Returns the handle for |obj|, or -1 if no handle exists.
  function getHandleForObject(obj) {
      for (var property in domAutomation.objects) {
        if (domAutomation.objects[property] == obj)
          return parseInt(property);
      }
      return -1;
  }

  // Sends a completed response back to the domAutomationController with a
  // return value, which can be of any type.
  function sendCompletedResponse(returnValue) {
    var result = [true, "", getConvertedValue(returnValue)];
    domAutomationController.sendJSON(JSON.stringify(result));
  }

  // Sends a error response back to the domAutomationController. |exception|
  // should be a string or an exception.
  function sendErrorResponse(exception) {
    var message = exception.message;
    if (typeof message == "undefined")
      message = exception;
    if (typeof message != "string")
      message = JSON.stringify(message);
    var result = [false, message, exception];
    domAutomationController.sendJSON(JSON.stringify(result));
  }

  // Safely evaluates |javascript| and sends a response back via the
  // DomAutomationController. See javascript_execution_controller.cc
  // for more details.
  domAutomation.evaluateJavaScript = function(javascript) {
    try {
      sendCompletedResponse(eval(javascript));
    }
    catch (exception) {
      sendErrorResponse(exception);
    }
  }

  // Called by a function when it has completed successfully. Any value,
  // including undefined, is acceptable for |returnValue|. This should only
  // be used by functions with an asynchronous response.
  function onAsyncJavaScriptComplete(callId, returnValue) {
    if (domAutomation.currentCallId != callId) {
      // We are not waiting for a response for this call anymore,
      // because it already responded.
      return;
    }
    domAutomation.currentCallId++;
    window.clearTimeout(domAutomation.currentTimeout);
    sendCompletedResponse(returnValue);
  }

  // Calld by a function when it has an error preventing its successful
  // execution. |exception| should be an exception or a string.
  function onAsyncJavaScriptError(callId, exception) {
    if (domAutomation.currentCallId != callId) {
      // We are not waiting for a response for this call anymore,
      // because it already responded.
      return;
    }
    domAutomation.currentCallId++;
    window.clearTimeout(domAutomation.currentTimeout);
    sendErrorResponse(exception);
  }

  // Returns whether the call with the given ID has already finished. If true,
  // this means that the call timed out or that it already gave a response.
  function didCallFinish(callId) {
    return domAutomation.currentCallId != callId;
  }

  // Safely evaluates |javascript|. The JavaScript is expected to return
  // a response via either onAsyncJavaScriptComplete or
  // onAsyncJavaScriptError. The script should respond within the |timeoutMs|.
  domAutomation.evaluateAsyncJavaScript = function(javascript, timeoutMs) {
    try {
      eval(javascript);
    }
    catch (exception) {
      onAsyncJavaScriptError(domAutomation.currentCallId, exception);
      return;
    }
    domAutomation.currentTimeout = window.setTimeout(
        onAsyncJavaScriptError, timeoutMs, domAutomation.currentCallId,
        "JavaScript timed out waiting for response.");
  }

  // Stops tracking the object associated with |handle|.
  domAutomation.removeObject = function(handle) {
    delete domAutomation.objects[handle];
  }

  // Stops tracking all objects.
  domAutomation.removeAll = function() {
    domAutomation.objects = {};
    domAutomation.nextHandle = 1;
  }

  // Gets the object associated with this |handle|.
  domAutomation.getObject = function(handle) {
    var obj = domAutomation.objects[handle]
    if (typeof obj == "undefined") {
      throw "Object with handle " + handle + " does not exist."
    }
    return domAutomation.objects[handle];
  }

  // Gets the ID for this asynchronous call.
  domAutomation.getCallId = function() {
    return domAutomation.currentCallId;
  }

  // Converts an indexable list with a length property to an array.
  function getArray(list) {
    var arr = [];
    for (var i = 0; i < list.length; i++) {
      arr.push(list[i]);
    }
    return arr;
  }

  // Removes whitespace at the beginning and end of |text|.
  function trim(text) {
    return text.replace(/^\s+|\s+$/g, "");
  }

  // Returns the window (which is a sub window of |win|) which
  // directly contains |doc|. May return null.
  function findWindowForDocument(win, doc) {
    if (win.document == doc)
      return win;
    for (var i = 0; i < win.frames.length; i++) {
      if (findWindowForDocument(win.frames[i], doc))
        return win.frames[i];
    }
    return null;
  }

  // Returns |element|'s text. This includes all descendants' text.
  // For textareas and inputs, the text is the element's value. For Text,
  // it is the textContent.
  function getText(element) {
    if (element instanceof Text) {
      return trim(element.textContent);
    } else if (element instanceof HTMLTextAreaElement ||
               element instanceof HTMLInputElement) {
      return element.value || "";
    }
    var childrenText = "";
    for (var i = 0; i < element.childNodes.length; i++) {
      childrenText += getText(element.childNodes[i]);
    }
    return childrenText;
  }

  // Returns whether |element| is visible.
  function isVisible(element) {
    while (element.style) {
      if (element.style.display == 'none' ||
          element.style.visibility == 'hidden' ||
          element.style.visibility == 'collapse') {
        return false;
      }
      element = element.parentNode;
    }
    return true;
  }

  // Returns an array of the visible elements found in the |elements| array.
  function getVisibleElements(elements) {
    var visibleElements = [];
    for (var i = 0; i < elements.length; i++) {
      if (isVisible(elements[i]))
        visibleElements.push(elements[i]);
    }
    return visibleElements;
  }

  // Finds all the elements which satisfy the xpath query using the context
  // node |context|. This function may throw an exception.
  function findByXPath(context, xpath) {
    var xpathResult =
        document.evaluate(xpath, context, null,
                          XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
    var elements = [];
    for (var i = 0; i < xpathResult.snapshotLength; i++) {
      elements.push(xpathResult.snapshotItem(i));
    }
    return elements;
  }

  // Finds the first element which satisfies the xpath query using the context
  // node |context|. This function may throw an exception.
  function find1ByXPath(context, xpath) {
    var xpathResult =
        document.evaluate(xpath, context, null,
                          XPathResult.FIRST_ORDERED_NODE_TYPE, null);
    return xpathResult.singleNodeValue;
  }

  // Finds all the elements which satisfy the selectors query using the context
  // node |context|. This function may throw an exception.
  function findBySelectors(context, selectors) {
    return getArray(context.querySelectorAll(selectors));
  }

  // Finds the first element which satisfies the selectors query using the
  // context node |context|. This function may throw an exception.
  function find1BySelectors(context, selectors) {
    return context.querySelector(selectors);
  }

  // Finds all the elements which contain |text| using the context
  // node |context|. See getText for details about what constitutes the text
  // of an element. This function may throw an exception.
  function findByText(context, text) {
    // Find all elements containing this text and all inputs containing
    // this text.
    var xpath = ".//*[contains(text(), '" + text + "')] | " +
                ".//input[contains(@value, '" + text + "')]";
    var elements = findByXPath(context, xpath);

    // Limit to what is visible.
    return getVisibleElements(elements);
  }

  // Finds the first element which contains |text| using the context
  // node |context|. See getText for details about what constitutes the text
  // of an element. This function may throw an exception.
  function find1ByText(context, text) {
    var elements = findByText(context, text);
    if (elements.length > 0)
      return findByText(context, text)[0];
    return null;
  }

  //// DOM Element automation methods
  //// See dom_element_proxy.h for method details.

  domAutomation.getDocumentFromFrame = function(element, frameNames) {
    // Find the window this element is in.
    var containingDocument = element.ownerDocument || element;
    var frame = findWindowForDocument(window, containingDocument);

    for (var i = 0; i < frameNames.length; i++) {
      frame = frame.frames[frameNames[i]];
      if (typeof frame == "undefined" || !frame) {
        return null;
      }
    }
    return frame.document;
  }

  domAutomation.findElement = function(context, query) {
    var type = query.type;
    var queryString = query.queryString;
    if (type == "xpath") {
      return find1ByXPath(context, queryString);
    } else if (type == "selectors") {
      return find1BySelectors(context, queryString);
    } else if (type == "text") {
      return find1ByText(context, queryString);
    }
  }

  domAutomation.findElements = function(context, query) {
    var type = query.type;
    var queryString = query.queryString;
    if (type == "xpath") {
      return findByXPath(context, queryString);
    } else if (type == "selectors") {
      return findBySelectors(context, queryString);
    } else if (type == "text") {
      return findByText(context, queryString);
    }
  }

  domAutomation.waitForVisibleElementCount = function(context, query, count,
                                                      callId) {
    (function waitHelper() {
      try {
        var elements = domAutomation.findElements(context, query);
        var visibleElements = getVisibleElements(elements);
        if (visibleElements.length == count)
          onAsyncJavaScriptComplete(callId, visibleElements);
        else if (!didCallFinish(callId))
          window.setTimeout(waitHelper, 500);
      }
      catch (exception) {
        onAsyncJavaScriptError(callId, exception);
      }
    })();
  }

  domAutomation.click = function(element) {
    var evt = document.createEvent('MouseEvents');
    evt.initMouseEvent('click', true, true, window,
                       0, 0, 0, 0, 0, false, false,
                       false, false, 0, null);
    while (element) {
      element.dispatchEvent(evt);
      element = element.parentNode;
    }
  }

  domAutomation.type = function(element, text) {
    if (element instanceof HTMLTextAreaElement ||
        (element instanceof HTMLInputElement && element.type == "text")) {
      element.value += text;
      return true;
    }
    return false;
  }

  domAutomation.setText = function(element, text) {
    if (element instanceof HTMLTextAreaElement ||
        (element instanceof HTMLInputElement && element.type == "text")) {
      element.value = text;
      return true;
    }
    return false;
  }

  domAutomation.getProperty = function(element, property) {
    return element[property];
  }

  domAutomation.getAttribute = function(element, attribute) {
    return element.getAttribute(attribute);
  }

  domAutomation.getValue = function(element, type) {
    if (type == "text") {
      return getText(element);
    } else if (type == "innerhtml") {
      return trim(element.innerHTML);
    } else if (type == "innertext") {
      return trim(element.innerText);
    } else if (type == "visibility") {
      return isVisible(element);
    } else if (type == "id") {
      return element.id;
    } else if (type == "contentdocument") {
      return element.contentDocument;
    }
  }

  domAutomation.waitForAttribute = function(element, attribute, value, callId) {
    (function waitForAttributeHelper() {
      try {
        if (element.getAttribute(attribute) == value)
          onAsyncJavaScriptComplete(callId);
        else if (!didCallFinish(callId))
          window.setTimeout(waitForAttributeHelper, 200);
      }
      catch (exception) {
        onAsyncJavaScriptError(callId, exception);
      }
    })();
  }
})();
