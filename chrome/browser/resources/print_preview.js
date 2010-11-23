// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();

/**
 * Window onload handler, sets up the page.
 */
function load() {
  $('cancel-button').addEventListener('click', function(e) {
    window.close();
  });

  chrome.send('getPrinters');
};

/**
 * Fill the printer list drop down.
 */
function setPrinters(printers) {
  if (printers.length > 0) {
    for (var i = 0; i < printers.length; ++i) {
      var option = document.createElement('option');
      option.textContent = printers[i];
      $('printer-list').add(option);
    }
  } else {
    var option = document.createElement('option');
    option.textContent = localStrings.getString('noPrinter');
    $('printer-list').add(option);
    $('printer-list').disabled = true;
    $('print-button').disabled = true;
  }
}

window.addEventListener('DOMContentLoaded', load);

