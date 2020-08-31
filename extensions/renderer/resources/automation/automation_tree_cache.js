// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');

function AutomationTreeCacheImpl() {}

function AutomationTreeCache() {
  privates(AutomationTreeCache).constructPrivate(this, arguments);
}

utils.defineProperty(
    AutomationTreeCache, 'idToAutomationRootNode', {__proto__: null});

exports.$set('AutomationTreeCache', AutomationTreeCache);
