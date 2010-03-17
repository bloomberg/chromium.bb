// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
  domAutomation.nextHandle = 1;

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

  // Safely evaluates |javascript| and sends a response back via the
  // DomAutomationController. See javascript_execution_controller.cc
  // for more details.
  domAutomation.evaluateJavaScript = function(javascript) {
    try {
      var result = [true, "", getConvertedValue(eval(javascript))];
    }
    catch (exception) {
      var message = exception.message;
      if (typeof message != "string")
        message = JSON.stringify(message);
      var result = [false, message, exception];
    }
    domAutomationController.sendJSON(JSON.stringify(result));
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
    return domAutomation.objects[handle];
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

  domAutomation.findByXPath = function(context, xpath) {
    var xpathResult =
        document.evaluate(xpath, context, null,
                          XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
    var elements = [];
    for (var i = 0; i < xpathResult.snapshotLength; i++) {
      elements.push(xpathResult.snapshotItem(i));
    }
    return elements;
  }

  domAutomation.find1ByXPath = function(context, xpath) {
    var xpathResult =
        document.evaluate(xpath, context, null,
                          XPathResult.FIRST_ORDERED_NODE_TYPE, null);
    return xpathResult.singleNodeValue;
  }

  domAutomation.findBySelectors = function(context, selectors) {
    return getArray(context.querySelectorAll(selectors));
  }

  domAutomation.find1BySelectors = function(context, selectors) {
    return context.querySelector(selectors);
  }

  domAutomation.findByText = function(context, text) {
    // Find all elements containing this text and all inputs containing
    // this text.
    var xpath = ".//*[contains(text(), '" + text + "')] | " +
                ".//input[contains(@value, '" + text + "')]";
    var elements = domAutomation.findByXPath(context, xpath);

    // Limit to what is visible.
    var final_list = [];
    for (var i = 0; i < elements.length; i++) {
      if (domAutomation.isVisible(elements[i]))
        final_list.push(elements[i]);
    }
    return final_list;
  }

  domAutomation.find1ByText = function(context, text) {
    return domAutomation.findByText(context, text)[0];
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

  domAutomation.getText = function(element) {
    if (element instanceof Text) {
      return trim(element.textContent);
    }
    else if (element instanceof HTMLTextAreaElement ||
            (element instanceof HTMLInputElement)) {
      return element.value || "";
    }
    var childrenText = "";
    for (var i = 0; i < element.childNodes.length; i++) {
      childrenText += domAutomation.getText(element.childNodes[i]);
    }
    return childrenText;
  }

  domAutomation.getInnerHTML = function(element) {
    return trim(element.innerHTML);
  }

  domAutomation.isVisible = function(element) {
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
})();
