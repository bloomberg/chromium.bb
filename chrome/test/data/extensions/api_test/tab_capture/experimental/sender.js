// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var colors = [ [ 255, 0, 0 ], [ 0, 255, 0 ], [ 0, 0, 255 ] ];

function rotateColorFills(colorIdx) {
  document.body.style.backgroundColor = "rgb(" + colors[colorIdx] + ")";
  setTimeout(rotateColorFills, 100, (colorIdx + 1) % colors.length);
}

rotateColorFills(0);
