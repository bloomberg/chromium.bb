// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

var middleButtonMouseDownTarget = null;

document.addEventListener('mousedown', function(e) {
  if (e.button == 1)
    middleButtonMouseDownTarget = e.path[0];
}, true);

document.addEventListener('mouseup', function(e) {
  if (e.button != 1)
    return;

  if (e.path.length > 0 && e.path[0] == middleButtonMouseDownTarget)
    middleButtonMouseDownTarget.dispatchEvent(new MouseEvent('click', e));

  middleButtonMouseDownTarget = null;
}, true);

})();
