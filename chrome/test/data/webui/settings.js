// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions for settings WebUI page.
function refreshPage() {
  window.location.reload();
}

function openUnderTheHood() {
  var item = $('advancedPageNav');
  assertTrue(item != null);
  assertTrue(item.onclick != null);
  item.onclick();
}

// Tests.
function testSetBooleanPrefTriggers() {
  // TODO(dtseng): make generic to click all buttons.
  var showHomeButton = $('toolbarShowHomeButton');
  assertTrue(showHomeButton != null);
  showHomeButton.click();
  showHomeButton.blur();
}

function testPageIsUnderTheHood() {
  var pageInstance = AdvancedOptions.getInstance();
  var topPage = OptionsPage.getTopmostVisiblePage();
  var expectedTitle = pageInstance.title;
  var actualTitle = document.title;
  assertEquals("chrome://settings/advanced", document.location.href);
  assertEquals(expectedTitle, actualTitle);
  assertEquals(pageInstance, topPage);
}
