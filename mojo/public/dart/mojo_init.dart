// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

library mojo_init;

import 'dart:async';

import 'core.dart' as core;
import 'dart-ext:src/mojo_dart_init';

void _init() native "MojoLibrary_Init";
void _mojoSystemThunksMake(Function fn) native "MojoSystemThunks_Make";

Future<Isolate> mojoInit() {
  _init();
  _mojoSystemThunksMake(core.mojoSystemThunksSet);
  return core.MojoHandleWatcher.Start();
}
