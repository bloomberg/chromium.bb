// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_TEST_WEBRUNNER_BROWSER_TEST_H_
#define FUCHSIA_ENGINE_TEST_WEBRUNNER_BROWSER_TEST_H_

#include <lib/fidl/cpp/binding_set.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

namespace cr_fuchsia {
namespace test {

// Base test class used for testing the WebRunner Context FIDL service in
// integration.
class WebRunnerBrowserTest : public content::BrowserTestBase {
 public:
  WebRunnerBrowserTest();
  ~WebRunnerBrowserTest() override;

  // Sets the Context client channel which will be bound to a Context FIDL
  // object by WebRunnerBrowserTest.
  static void SetContextClientChannel(zx::channel channel);

  // Creates a Frame for this Context.
  // |observer|: If set, specifies the navigation observer for the Frame.
  chromium::web::FramePtr CreateFrame(
      chromium::web::NavigationEventObserver* observer);

  // Gets the client object for the Context service.
  chromium::web::ContextPtr& context() { return context_; }

  // Gets the underlying ContextImpl service instance.
  ContextImpl* context_impl() const;

  fidl::BindingSet<chromium::web::NavigationEventObserver>&
  navigation_observer_bindings() {
    return navigation_observer_bindings_;
  }

  void set_test_server_root(const base::FilePath& path) {
    test_server_root_ = path;
  }

  // content::BrowserTestBase implementation.
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  base::FilePath test_server_root_;
  chromium::web::ContextPtr context_;
  fidl::BindingSet<chromium::web::NavigationEventObserver>
      navigation_observer_bindings_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerBrowserTest);
};

}  // namespace test
}  // namespace cr_fuchsia

#endif  // FUCHSIA_ENGINE_TEST_WEBRUNNER_BROWSER_TEST_H_
