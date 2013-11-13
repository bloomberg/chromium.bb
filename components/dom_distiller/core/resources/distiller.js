// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
  // TODO(bengr): Replace this JavaScript, which is only a placeholder, with
  //              code that will extract article content from a loaded page.
  var result = new Array(4);
  result[0] = "Rain in Seattle";
  result[1] = "<i>Seattle</i> It is raining. " +
              "<img src='http://www.a.com/img0.jpg' id='0'>" +
              "<img src='http://www.a.com/img1.jpg' id='1'>";
  result[2] = "http://www.a.com/img0.jpg";
  result[3] = "http://www.a.com/img1.jpg";
  return result;
}())
