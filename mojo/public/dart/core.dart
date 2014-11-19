// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

library core;

import 'dart:async';
import 'dart:core';
import 'dart:isolate';
import 'dart:typed_data';
import 'dart-ext:src/mojo_dart_core';

part 'src/buffer.dart';
part 'src/data_pipe.dart';
part 'src/handle.dart';
part 'src/handle_watcher.dart';
part 'src/message_pipe.dart';
part 'src/types.dart';

void mojoSystemThunksSet(int thunks) native "MojoSystemThunks_Set";
