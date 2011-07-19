// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* All the work we do onload. */
function onLoadWork() {
  // This is the javascript code that processes the template:
  var input = new JsEvalContext(templateData);
  var output = document.getElementById('t');
  jstProcess(input, output);
}

document.addEventListener('DOMContentLoaded', onLoadWork);

