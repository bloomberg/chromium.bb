// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "cc/raster/single_thread_task_graph_runner.h"

namespace blimp {
namespace client {

namespace {
class BlimpTaskGraphRunner : public cc::SingleThreadTaskGraphRunner {
 public:
  BlimpTaskGraphRunner() {
    Start("BlimpCompositorWorker",
          base::SimpleThread::Options(base::ThreadPriority::BACKGROUND));
  }

  ~BlimpTaskGraphRunner() override { Shutdown(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpTaskGraphRunner);
};
}  // namespace

BlimpCompositorDependencies::BlimpCompositorDependencies(
    std::unique_ptr<CompositorDependencies> embedder_dependencies)
    : embedder_dependencies_(std::move(embedder_dependencies)) {
  DCHECK(embedder_dependencies_);
}

BlimpCompositorDependencies::~BlimpCompositorDependencies() {
  if (compositor_impl_thread_)
    compositor_impl_thread_->Stop();
}

CompositorDependencies* BlimpCompositorDependencies::GetEmbedderDependencies() {
  return embedder_dependencies_.get();
}

cc::TaskGraphRunner* BlimpCompositorDependencies::GetTaskGraphRunner() {
  if (!task_graph_runner_)
    task_graph_runner_ = base::MakeUnique<BlimpTaskGraphRunner>();

  return task_graph_runner_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
BlimpCompositorDependencies::GetCompositorTaskRunner() {
  if (!compositor_impl_thread_) {
    // Lazily build the compositor thread.
    base::Thread::Options thread_options;
#if defined(OS_ANDROID)
    thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
    compositor_impl_thread_ =
        base::MakeUnique<base::Thread>("Blimp Compositor Impl");
    compositor_impl_thread_->StartWithOptions(thread_options);

    compositor_impl_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed),
                   false));
  }

  return compositor_impl_thread_->task_runner();
}

cc::ImageSerializationProcessor*
BlimpCompositorDependencies::GetImageSerializationProcessor() {
  return BlobImageSerializationProcessor::current();
}

}  // namespace client
}  // namespace blimp
