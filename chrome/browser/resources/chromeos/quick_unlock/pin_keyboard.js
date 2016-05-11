// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'pin-keyboard',

  // Called when a keypad number has been tapped.
  onNumberTap_: function(event, detail) {
    var target = event.target;
    var value = target.getAttribute('value');

    var input = this.$$('#pin-input');
    input.value += value;
  },

  // Called when the user wants to submit the PIN.
  onPinSubmit_: function() {
    var pin = this.$$('#pin-input').value;
    this.fire('submit', { pin: pin });
  },

  // Called when a key event is pressed while the input element has focus.
  onInputKeyDown_: function(event) {
    // Up/down pressed, swallow the event to prevent the input value from
    // being incremented or decremented.
    if (event.keyCode == 38 || event.keyCode == 40)
      event.preventDefault();

    // Enter pressed.
    if (event.keyCode == 13) {
      this.onPinSubmit_();
      event.preventDefault();
    }
  }
});
