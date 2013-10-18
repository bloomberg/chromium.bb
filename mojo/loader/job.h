// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_LOADER_JOB_H_
#define MOJO_LOADER_JOB_H_

#include "url/gurl.h"

namespace mojo {
namespace loader {

// A job represents an individual network load operation.
class Job {
 public:
  class Delegate {
   public:
    virtual void DidCompleteLoad(const GURL& app_url) = 0;

   protected:
    virtual ~Delegate();
  };

  // You can cancel a job by deleting it.
  virtual ~Job();

 protected:
  // You can create a job using Loader.
  Job();

  DISALLOW_COPY_AND_ASSIGN(Job);
};

}  // namespace loader
}  // namespace mojo

#endif  // MOJO_LOADER_JOB_H_
