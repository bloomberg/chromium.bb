// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testLogText() {
  var divLogs = document.getElementById('log-entries');
  assertNotEquals(null, divLogs, "The <div> with logs not found.");
  assertNotEquals(null,
                  divLogs.innerHTML.match(/text for testing/),
                  "The logged text not found.");
  assertEquals(null,
               divLogs.innerHTML.match(/<script>/),
               "The logged text was not escaped.");
}

function testLogTextNotPresent() {
  var divLogs = document.getElementById('log-entries');
  assertNotEquals(null, divLogs, "The <div> with logs not found.");
  assertEquals(null,
               divLogs.innerHTML.match(/text for testing/),
               "The logged text was found, but should not.");
}
