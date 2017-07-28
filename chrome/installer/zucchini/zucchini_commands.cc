// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini_commands.h"

#include "base/logging.h"

namespace {

// TODO(huangs): File I/O utilities.

}  // namespace

zucchini::status::Code MainGen(const base::CommandLine& command_line,
                               const std::vector<base::FilePath>& fnames) {
  CHECK_EQ(3U, fnames.size());
  // TODO(etiennep): Implement.
  return zucchini::status::kStatusSuccess;
}

zucchini::status::Code MainApply(const base::CommandLine& command_line,
                                 const std::vector<base::FilePath>& fnames) {
  CHECK_EQ(3U, fnames.size());
  // TODO(etiennep): Implement.
  return zucchini::status::kStatusSuccess;
}
