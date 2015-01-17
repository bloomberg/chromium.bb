// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var binding = require('binding').Binding.create('printerProvider');
var printerProviderInternal = require('binding').Binding.create(
    'printerProviderInternal').generate();
var eventBindings = require('event_bindings');

var printerProviderSchema =
    requireNative('schema_registry').GetSchema('printerProvider')
var utils = require('utils');
var validate = require('schemaUtils').validate;

// Custom bindings for chrome.printerProvider API.
// The bindings are used to implement callbacks for the API events. Internally
// each event is passed requestId argument used to identify the callback
// associated with the event. This argument is massaged out from the event
// arguments before dispatching the event to consumers. A callback is appended
// to the event arguments. The callback wraps an appropriate
// chrome.printerProviderInternal API function that is used to report the event
// result from the extension. The function is passed requestId and values
// provided by the extension. It validates that the values provided by the
// extension match chrome.printerProvider event callback schemas. It also
// ensures that a callback is run at most once. In case there is an exception
// during event dispatching, the chrome.printerProviderInternal function
// is called with a default error value.
//

// Handles a chrome.printerProvider event as described in the file comment.
// |eventName|: The event name.
// |resultreporter|: The function that should be called to report event result.
//                   One of chrome.printerProviderInternal API functions.
function handleEvent(eventName, resultReporter) {
  eventBindings.registerArgumentMassager(
      'printerProvider.' + eventName,
      function(args, dispatch) {
        var responded = false;

        // Validates that the result passed by the extension to the event
        // callback matches the callback schema. Throws an exception in case of
        // an error.
        var validateResult = function(result) {
          var eventSchema =
              utils.lookup(printerProviderSchema.events, 'name', eventName);
          var callbackSchema =
              utils.lookup(eventSchema.parameters, 'type', 'function');

          validate([result], callbackSchema.parameters);
        };

        // Function provided to the extension as the event callback argument.
        // It makes sure that the event result hasn't previously been returned
        // and that the provided result matches the callback schema. In case of
        // an error it throws an exception.
        var reportResult = function(result) {
          if (responded) {
            throw new Error(
                'Event callback must not be called more than once.');
          }

          var finalResult = null;
          try {
            validateResult(result);  // throws on failure
            finalResult = result;
          } finally {
            responded = true;
            resultReporter(args[0] /* requestId */, finalResult);
          }
        };

        dispatch(args.slice(1).concat(reportResult));
      });
}

handleEvent('onGetPrintersRequested', printerProviderInternal.reportPrinters);

handleEvent('onGetCapabilityRequested',
            printerProviderInternal.reportPrinterCapability);

handleEvent('onPrintRequested', printerProviderInternal.reportPrintResult);

exports.binding = binding.generate();
