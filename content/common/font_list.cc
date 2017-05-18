// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include "base/lazy_instance.h"
#include "base/task_scheduler/post_task.h"

namespace content {

namespace {

struct FontListTaskRunner {
  const scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
};

base::LazyInstance<FontListTaskRunner>::Leaky g_font_list_task_runner =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

scoped_refptr<base::SequencedTaskRunner> GetFontListTaskRunner() {
  return g_font_list_task_runner.Get().task_runner;
}

}  // content
