// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('DomInitializer');

goog.require('image.collections.extension.domextractor.DomController');

var domController =
    new image.collections.extension.domextractor.DomController();
domController.initialize();
