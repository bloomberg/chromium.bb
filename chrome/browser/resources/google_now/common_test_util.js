// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common test utilities.

function emptyMock() {}

// Container for event handlers added by mocked 'addListener' functions.
var mockEventHandlers = {};

/**
 * Mocks 'addListener' function of an API event. The mocked function will keep
 * track of handlers.
 * @param {Object} topLevelContainer Top-level container of the original
 *     function. Can be either 'chrome' or 'instrumented'.
 * @param {string} eventIdentifier Event identifier, such as
 *     'runtime.onSuspend'.
 */
function mockChromeEvent(topLevelContainer, eventIdentifier) {
  var eventIdentifierParts = eventIdentifier.split('.');
  var eventName = eventIdentifierParts.pop();
  var originalMethodContainer = topLevelContainer;
  var mockEventContainer = mockEventHandlers;
  eventIdentifierParts.forEach(function(fragment) {
    originalMethodContainer =
        originalMethodContainer[fragment] =
        originalMethodContainer[fragment] || {};
    mockEventContainer =
        mockEventContainer[fragment] =
        mockEventContainer[fragment] || {};
  });

  mockEventContainer[eventName] = [];
  originalMethodContainer[eventName] = {
    addListener: function(callback) {
      mockEventContainer[eventName].push(callback);
    }
  };
}

/**
 * Gets the array of event handlers added by a mocked 'addListener' function.
 * @param {string} eventIdentifier Event identifier, such as
 *     'runtime.onSuspend'.
 * @return {Array.<Function>} Array of handlers.
 */
function getMockHandlerContainer(eventIdentifier) {
  var eventIdentifierParts = eventIdentifier.split('.');
  var mockEventContainer = mockEventHandlers;
  eventIdentifierParts.forEach(function(fragment) {
    mockEventContainer = mockEventContainer[fragment];
  });

  return mockEventContainer;
}
