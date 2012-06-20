// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  try {
    window.alert('should throw');
    window.parent.postMessage({'success': false,
                               'reason' : 'should have thrown'},
                              '*');
  } catch(e) {
    var message = e.message || e;
    var succ = message.indexOf('is not available in packaged apps') != -1;
    window.parent.postMessage({'success': succ,
                               'reason' : 'got wrong error: ' + message},
                              '*');

  }
};
