// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Code shared by some of the visibility tests.  Maintains a list of
// visibility states, starting with the state when this file was loaded.
// New states are added on each visibility change event.

// Array of previously observed visibility states.
var visibilityStates = [document.webkitVisibilityState];

// Array of previously observed hidden values.
var hiddenValues = [document.webkitHidden];

// Record all visibility changes in corresponding arrays.
function onVisibilityChange(event) {
  visibilityStates.push(document.webkitVisibilityState);
  hiddenValues.push(document.webkitHidden);
}

document.addEventListener("webkitvisibilitychange",
                          onVisibilityChange,
                          false);

