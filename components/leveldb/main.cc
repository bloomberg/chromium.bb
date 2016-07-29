// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/leveldb/leveldb_app.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/service_runner.h"

MojoResult ServiceMain(MojoHandle application_request) {
  shell::ServiceRunner runner(new leveldb::LevelDBApp());
  return runner.Run(application_request);
}
