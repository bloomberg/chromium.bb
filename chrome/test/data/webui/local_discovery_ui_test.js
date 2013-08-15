// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function tableRows() {
  return document.querySelectorAll('#devices-table tr');
}

function checkTableHasOneRow() {
  var rows = tableRows();
  assertEquals(2, rows.length);
  var firstRow = rows.item(1);

  var td = firstRow.firstChild;
  assertEquals('myService._privet._tcp.local', td.textContent);

  td = td.nextSibling;
  assertEquals('myservice.local', td.textContent);

  td = td.nextSibling;
  assertEquals('8888', td.textContent);

  td = td.nextSibling;
  assertEquals('1.2.3.4', td.textContent);

  td = td.nextSibling;
  assertEquals('unknown', td.textContent);

  td = td.nextSibling;
  assertEquals('Registered', td.textContent);
}

function checkTableHasNoRows() {
  var rows = tableRows();
  assertEquals(1, rows.length);
}
