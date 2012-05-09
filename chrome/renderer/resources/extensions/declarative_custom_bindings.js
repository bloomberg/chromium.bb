// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the declarative API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;

chromeHidden.registerCustomHook('declarative',
                                function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  var apiDefinitions = bindingsAPI.apiDefinitions;
  var cachedEventOptions = {};

  function getEventOptions(qualifiedEventName) {
    if (cachedEventOptions[qualifiedEventName])
      return cachedEventOptions[qualifiedEventName];

    // Parse qualifiedEventName into namespace and event name.
    var lastSeparator = qualifiedEventName.lastIndexOf('.');
    var eventName = qualifiedEventName.substr(lastSeparator + 1);
    var namespace = qualifiedEventName.substr(0, lastSeparator);

    // Lookup schema definition.
    var filterNamespace = function(val) {return val.namespace === namespace;};
    var apiSchema = apiDefinitions.filter(filterNamespace)[0];
    var filterEventName = function (val) {return val.name === eventName;};
    var eventSchema = apiSchema.events.filter(filterEventName)[0];

    cachedEventOptions[qualifiedEventName] = eventSchema.options;
    return eventSchema.options;
  }

  // Takes a list of JSON datatype identifiers and returns a schema fragment
  // that verifies that a JSON object corresponds to an array of only these
  // data types.
  function buildArrayOfChoicesSchema(typesList) {
    return {
      'type': 'array',
      'items': {
        'choices': typesList.map(function(el) {return {'$ref': el};})
      }
    };
  }

  // Validate conditions and actions against specific schemas of this
  // event object type.
  // |rules| is an array of JSON objects that follow the Rule type of the
  // declarative extension APIs. |conditions| is an array of JSON type
  // identifiers that are allowed to occur in the conditions attribute of each
  // rule. Likewise, |actions| is an array of JSON type identifiers that are
  // allowed to occur in the actions attribute of each rule.
  function validateRules(rules, conditions, actions) {
    var conditionsSchema = buildArrayOfChoicesSchema(conditions);
    var actionsSchema = buildArrayOfChoicesSchema(actions);
    rules.forEach(function(rule) {
      chromeHidden.validate([rule.conditions], [conditionsSchema]);
      chromeHidden.validate([rule.actions], [actionsSchema]);
    })
  }

  apiFunctions.setHandleRequest('addRules',
                                function(eventName, rules, opt_callback) {
    var eventOptions = getEventOptions(eventName);
    if (!eventOptions.conditions || !eventOptions.actions) {
      throw new Error('Event ' + eventName + ' misses conditions or ' +
                      'actions in the API specification.');
    }
    validateRules(rules,
                  eventOptions.conditions,
                  eventOptions.actions);
    sendRequest(this.name, [eventName, rules, opt_callback],
                this.definition.parameters);
  });
});
