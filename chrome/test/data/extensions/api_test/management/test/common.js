// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertNoLastError = chrome.test.assertNoLastError;
var assertTrue = chrome.test.assertTrue;
var fail = chrome.test.fail;
var succeed = chrome.test.succeed;
var listenOnce = chrome.test.listenOnce;
var callback = chrome.test.callback;

function getItemNamed(list, name) {
  for (var i = 0; i < list.length; i++) {
    if (list[i].name == name) {
      return list[i];
    }
  }
  fail("didn't find item with name: " + name);
  return null;
}

// Verifies that the item's name, enabled, and isApp properties match |name|,
// |enabled|, and |isApp|, and checks against any additional name/value
// properties from |additional_properties|.
function checkItem(item, name, enabled, isApp, additional_properties) {
  assertTrue(item !== null);
  assertEq(name, item.name);
  assertEq(isApp, item.isApp);
  assertEq(enabled, item.enabled);

  for (var propname in additional_properties) {
    var value = additional_properties[propname];
    if (typeof value === 'string')
      value = value.replace("<ID>", item.id);
    assertTrue(propname in item);
    assertEq(value, item[propname]);
  }
}

// Gets an extension/app with |name| in |list|, verifies that its enabled
// and isApp properties match |enabled| and |isApp|, and checks against any
// additional name/value properties from |additional_properties|.
function checkItemInList(list, name, enabled, isApp, additional_properties) {
  var item = getItemNamed(list, name);
  checkItem(item, name, enabled, isApp, additional_properties);
}
