/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var kb = document.getElementById('keyboard');

/**
 * Finds the character specified and types it. Assumes that the default
 * layout is qwerty, and the default keyset is lower.
 * @param {{string}} char The character to type.
 */
var type = function(char) {
  var keyset = kb.querySelector('#qwerty-lower');
  var keys = Array.prototype.slice.call(keyset.querySelectorAll('kb-key'));
  var key = keys.filter(function(key) {
    return key.charValue == char;
  })[0];
  key.down({pointerId: 1});
  key.up({pointerId: 1});
}

if (kb.isReady()) {
  type('a');
} else {
  kb.addKeysetChangedObserver(function() {
    if (kb.isReady())
      type('a');
  });
}