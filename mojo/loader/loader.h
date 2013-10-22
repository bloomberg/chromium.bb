// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_LOADER_LOADER_H_
#define MOJO_LOADER_LOADER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "mojo/loader/job.h"
#include "url/gurl.h"

namespace mojo {
namespace loader {

class Loader {
 public:
  Loader(base::SingleThreadTaskRunner* network_runner,
         base::SingleThreadTaskRunner* file_runner,
         base::FilePath base_path);
  ~Loader();

  scoped_ptr<Job> Load(const GURL& app_url, Job::Delegate* delegate);

 private:
  class Data;

  scoped_ptr<Data> data_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

}  // namespace loader
}  // namespace mojo

#endif  // MOJO_LOADER_LOADER_H_
