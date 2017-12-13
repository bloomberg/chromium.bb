// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Adds listeners that are used to handle forms, enabling autofill
 * and the replacement method to dismiss the keyboard needed because of the
 * Autofill keyboard accessory.
 */

goog.provide('__crWeb.form');

goog.require('__crWeb.message');

/** Beginning of anonymous object */
(function() {
  // Skip iframes that have the same origin as the main frame. For such frames
  // no form related actions (eg. filling, saving) are supported.
  try {
    // The following line generates exception for iframes that have different
    // origin that.
    // TODO(crbug.com/792642): implement sending messages instead of using
    // window.top, when messaging framework is ready.
    if (!window.top.document)
      return;
  }
  catch(error) {
    return;
  }


  /**
   * Focus and input events for form elements are messaged to the main
   * application for broadcast to WebStateObservers.
   * This is done with a single event handler for each type being added to the
   * main document element which checks the source element of the event; this
   * is much easier to manage than adding handlers to individual elements.
   * @private
   */
  var formActivity_ = function(evt) {
    var srcElement = evt.srcElement;
    var value = srcElement.value || '';
    var fieldType = srcElement.type || '';

    var msg = {
      'command': 'form.activity',
      'formName': __gCrWeb.common.getFormIdentifier(evt.srcElement.form),
      'fieldName': __gCrWeb.common.getFieldIdentifier(srcElement),
      'fieldType': fieldType,
      'type': evt.type,
      'value': value
    };
    __gCrWeb.message.invokeOnHost(msg);
  };

  /**
   * Focus events performed on the 'capture' phase otherwise they are often
   * not received.
   */
  document.addEventListener('focus', formActivity_, true);
  document.addEventListener('blur', formActivity_, true);
  document.addEventListener('change', formActivity_, true);

  /**
   * Text input is watched at the bubbling phase as this seems adequate in
   * practice and it is less obtrusive to page scripts than capture phase.
   */
  document.addEventListener('input', formActivity_, false);
  document.addEventListener('keyup', formActivity_, false);

  /**
   * Capture form submit actions.
   */
  document.addEventListener('submit', function(evt) {
    var action;
    if (evt['defaultPrevented'])
      return;
    action = evt.target.getAttribute('action');
    // Default action is to re-submit to same page.
    if (!action) {
      action = document.location.href;
    }
    __gCrWeb.message.invokeOnHost({
             'command': 'document.submit',
            'formName': __gCrWeb.common.getFormIdentifier(evt.srcElement),
                'href': getFullyQualifiedUrl_(action)
    });
  }, false);

  /** @private */
  var getFullyQualifiedUrl_ = function(originalURL) {
    // A dummy anchor (never added to the document) is used to obtain the
    // fully-qualified URL of |originalURL|.
    var anchor = document.createElement('a');
    anchor.href = originalURL;
    return anchor.href;
  };

  /** Flush the message queue. */
  if (__gCrWeb.message) {
    __gCrWeb.message.invokeQueues();
  }

}());  // End of anonymous object
