// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_PROCESS_OBSERVER_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_PROCESS_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_process_observer.h"
#include "ipc/ipc_platform_file.h"

namespace blink {
class WebFrame;
}

namespace test_runner {
class WebTestDelegate;
class WebTestInterfaces;
}

namespace content {

class RenderView;
class BlinkTestRunner;

class LayoutTestRenderProcessObserver : public RenderProcessObserver {
 public:
  static LayoutTestRenderProcessObserver* GetInstance();

  LayoutTestRenderProcessObserver();
  ~LayoutTestRenderProcessObserver() override;

  void SetTestDelegate(test_runner::WebTestDelegate* delegate);
  void SetMainWindow(RenderView* view);

  // RenderProcessObserver implementation.
  void WebKitInitialized() override;
  void OnRenderProcessShutdown() override;
  bool OnControlMessageReceived(const IPC::Message& message) override;

  test_runner::WebTestDelegate* test_delegate() const {
    return test_delegate_;
  }
  test_runner::WebTestInterfaces* test_interfaces() const {
    return test_interfaces_.get();
  }
  BlinkTestRunner* main_test_runner() const { return main_test_runner_; }
  const base::FilePath& webkit_source_dir() const { return webkit_source_dir_; }

 private:
  // Message handlers.
  void OnSetWebKitSourceDir(const base::FilePath& webkit_source_dir);

  BlinkTestRunner* main_test_runner_;
  test_runner::WebTestDelegate* test_delegate_;
  scoped_ptr<test_runner::WebTestInterfaces> test_interfaces_;

  base::FilePath webkit_source_dir_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestRenderProcessObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_PROCESS_OBSERVER_H_
