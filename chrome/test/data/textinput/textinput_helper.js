// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var textinput_helper = {
  retrieveElementCoordinate: function(id) {
    var ele = document.getElementById(id);
    var coordinate = ele.offsetLeft + ',' + ele.offsetTop + ',' +
        ele.offsetWidth + ',' + ele.offsetHeight;
    window.domAutomationController.send(coordinate);
  }
};
