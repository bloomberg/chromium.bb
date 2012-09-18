// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var branchChooser = document.getElementById('branchChooser');
  // No branch chooser on stable (for example).
  if (!branchChooser)
    return;

  branchChooser.addEventListener('change', function() {
    var value = event.target.value;
    if (!value)
      return;
    var current_branch = window.bootstrap.branchInfo.current;
    var path = window.location.pathname.split('/');
    if (path[0] == '')
      path = path.slice(1);
    var index = path.indexOf(current_branch);
    if (index != -1)
      path[index] = value;
    else
      path.splice(0, 0, value);
    window.location = '/' + path.join('/');
  });
})()
