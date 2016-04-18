// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_THREAD_OBSERVER_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_THREAD_OBSERVER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/renderer/render_thread_observer.h"
#include "ipc/ipc_platform_file.h"

namespace blink {
class WebFrame;
}

namespace test_runner {
class WebTestDelegate;
class WebTestInterfaces;
}

namespace content {

class LayoutTestRenderThreadObserver : public RenderThreadObserver {
 public:
  static LayoutTestRenderThreadObserver* GetInstance();

  LayoutTestRenderThreadObserver();
  ~LayoutTestRenderThreadObserver() override;

  void SetTestDelegate(test_runner::WebTestDelegate* delegate);

  // RenderThreadObserver implementation.
  void OnRenderProcessShutdown() override;
  bool OnControlMessageReceived(const IPC::Message& message) override;

  test_runner::WebTestDelegate* test_delegate() const {
    return test_delegate_;
  }
  test_runner::WebTestInterfaces* test_interfaces() const {
    return test_interfaces_.get();
  }
  const base::FilePath& webkit_source_dir() const { return webkit_source_dir_; }

 private:
  // Message handlers.
  void OnSetWebKitSourceDir(const base::FilePath& webkit_source_dir);

  test_runner::WebTestDelegate* test_delegate_;
  std::unique_ptr<test_runner::WebTestInterfaces> test_interfaces_;

  base::FilePath webkit_source_dir_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestRenderThreadObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_THREAD_OBSERVER_H_
