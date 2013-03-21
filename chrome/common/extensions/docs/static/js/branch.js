// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

function init() {
  var branchChooser = document.getElementById('branch-chooser-popup');
  Array.prototype.forEach.call(branchChooser.getElementsByTagName('button'),
                               function(button) {
    button.addEventListener('click', function(event) {
      choose(button.className);
      event.stopPropagation();
    });
  });
}

function choose(branch) {
  var currentBranch = window.bootstrap.branchInfo.current;
  var path = window.location.pathname.split('/');
  if (path[0] == '')
    path = path.slice(1);
  var index = path.indexOf(currentBranch);
  if (index != -1)
    path[index] = branch;
  else
    path.splice(0, 0, branch);
  window.location.pathname = '/' + path.join('/');
}

init();

})()
