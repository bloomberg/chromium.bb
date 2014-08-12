/* Copyright 2014 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/

/**
 * On load, registers the handler to add the 'hide' class to the container
 * element in order to hide it.
 */
window.onload = function() {
  $('optin-label').addEventListener('click', function() {
    $('container').classList.add('hide');
  });
};
