// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createEmptyState, reduceAction, Store} from 'chrome://bookmarks/bookmarks.js';
import {TestStore as CrUiTestStore} from 'chrome://test/test_store.m.js';

export class TestStore extends CrUiTestStore {
  constructor(data) {
    super(data, Store, createEmptyState(), reduceAction);
  }
}
