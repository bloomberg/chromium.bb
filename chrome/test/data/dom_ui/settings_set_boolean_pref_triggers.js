// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

runTests([
function() {
  // TODO(dtseng): make generic to click all buttons.
  var showHomeButton = $('toolbarShowHomeButton');
  assertTrue(showHomeButton != null);
  showHomeButton.click();
  showHomeButton.blur();
}
]);
