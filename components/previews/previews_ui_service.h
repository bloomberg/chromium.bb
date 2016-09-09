// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_PREVIEWS_UI_SERVICE_H_
#define COMPONENTS_PREVIEWS_PREVIEWS_UI_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"

namespace previews {
class PreviewsIOData;

// A class to manage the UI portion of inter-thread communication between
// previews/ objects. Created and used on the UI thread.
class PreviewsUIService {
 public:
  PreviewsUIService(
      PreviewsIOData* previews_io_data,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  virtual ~PreviewsUIService();

  // Sets |io_data_| to |io_data| to allow calls from the UI thread to the IO
  // thread. Virtualized in testing.
  virtual void SetIOData(base::WeakPtr<PreviewsIOData> io_data);

 private:
  // The IO thread portion of the inter-thread communication for previews/.
  base::WeakPtr<previews::PreviewsIOData> io_data_;

  base::ThreadChecker thread_checker_;

  // The IO thread task runner. Used to post tasks to |io_data_|.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::WeakPtrFactory<PreviewsUIService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsUIService);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_PREVIEWS_UI_SERVICE_H_
