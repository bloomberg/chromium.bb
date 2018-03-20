// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="../login/oobe_types.js">
// <include src="../login/oobe_buttons.js">
// <include src="../login/oobe_change_picture.js">
// <include src="../login/oobe_dialog.js">
// <include src="assistant_value_prop.js">

cr.define('assistantOptin', function() {
  return {

    // Starts the assistant opt-in flow.
    Show: function() {
      $('value-prop-md').locale = loadTimeData.getString('locale');
      $('value-prop-md').onShow();
    }
  };
});

document.addEventListener('DOMContentLoaded', function() {
  assistantOptin.Show();
});
