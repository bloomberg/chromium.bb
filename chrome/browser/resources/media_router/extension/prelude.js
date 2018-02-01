// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog = {};
goog.require = () => undefined;
goog.provide = goog.require;
goog.module = goog.require;
goog.forwardDeclare = goog.require;
goog.scope = body => {
  body.call(window);
};
