// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  function redirectToBranch() {
    var value = event.target.value;
    if (!value)
      return;
    var current_branch = window.bootstrap.branchInfo.current;
    var path = window.location.pathname.split('/');
    var index = path.indexOf(current_branch);
    if (index != -1)
      path[index] = value;
    else
      path.splice(path.length - 1, 0, value);
    window.location = path.join('/');
  }

  document.getElementById('branchChooser').addEventListener(
      'change',
      redirectToBranch);
})()
