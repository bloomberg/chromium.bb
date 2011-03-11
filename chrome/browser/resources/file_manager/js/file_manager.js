// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onLoad() {
  var args = '';
  var argstr = decodeURIComponent(document.location.search.substr(1));
  if (argstr)
    args = JSON.parse(argstr);

  document.body.appendChild(document.createTextNode(
      'dialog arguments: ' + JSON.stringify(args)));
}
