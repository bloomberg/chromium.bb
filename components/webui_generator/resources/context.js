// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global) {
  var has_cr = !!global.cr;
  var cr_define_bckp = null;
  if (has_cr && global.cr.define)
    cr_define_bckp = global.cr.define;
  if (!has_cr)
     global.cr = {}
  global.cr.define = function(_, define) {
    global.wug = global.wug || {}
    global.wug.Context = define().ScreenContext;
  }
<include src="../../../chrome/browser/resources/chromeos/login/screen_context.js">
  if (!has_cr)
    delete global.cr;
  if (cr_define_bckp)
    global.cr.define = cr_define_bckp;
})(this);
