// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.require('moduleB.TestInterface');

// This is not expected to do anything useful, but it must compile.
const proxy = moduleB.TestInterface.getProxy();
proxy.passA1({'q': '', 'r': '', 's': ''});
