// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function pageObjectEquals(a, b) {
  return a.url == b.url && a.title == b.title && a.pinned == b.pinned;
}

function pageObjectArrayEquals(a, b) {
  if (a.length != b.length)
    return false;

  for (var i = 0; i < a.length; i++) {
    if (!a && !b)
      continue;

    if (!pageObjectEquals(a[i], b[i]))
      return false;
  }

  return true;
}

function refreshDataBasic() {
  // Tests that we toss out pages that don't appear in the new data.
  var oldData = [{url: 'foo'}];
  var newData = [{url: 'bar'}];
  var mergedData = ntp.refreshData(oldData, newData);
  assertTrue(pageObjectArrayEquals(mergedData, newData));
}

function refreshDataOrdering() {
  // Tests that the order of pages (based on URL) in the current list is
  // preserved, but the other page data (like title) is copied from newData.
  var oldData = [
    {url: 'foo', title: 'foo (1)'},
    {url: 'bar', title: 'bar (1)'}];

  var newData = [
    {url: 'bar', title: 'bar (2)'},
    {url: 'foo', title: 'foo (2)'}];

  var expectedData = [
    {url: 'foo', title: 'foo (2)'},
    {url: 'bar', title: 'bar (2)'}];

  var mergedData = ntp.refreshData(oldData, newData);
  assertTrue(pageObjectArrayEquals(mergedData, expectedData));
}

function refreshDataPinning() {
  // Tests that the 'pinned' attribute is always respected.
  var oldData = [
    {url: 'foo', title: 'foo (1)'},
    {url: 'bar', title: 'bar (1)'}];

  var newData = [
    {url: 'bar', title: 'bar (2)', pinned: true},
    {url: 'foo', title: 'foo (2)'}];

  var mergedData = ntp.refreshData(oldData, newData);
  assertTrue(pageObjectArrayEquals(mergedData, newData));

  // Tests we don't choke on empty data before a pinned entry.
  oldData = [{url: 'foo', title: 'foo (1)'}];
  newData = [{}, {url: 'foo', title: 'foo (2)', pinned: true}];

  mergedData = ntp.refreshData(oldData, newData);
  assertTrue(pageObjectArrayEquals(mergedData, newData));
}
