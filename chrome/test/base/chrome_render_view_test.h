// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_
#define CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/renderer/chrome_mock_render_thread.h"
#include "content/public/test/render_view_test.h"

namespace autofill {
class AutofillAgent;
class TestPasswordAutofillAgent;
class TestPasswordGenerationAgent;
}

namespace extensions {
class DispatcherDelegate;
}

class ChromeRenderViewTest : public content::RenderViewTest {
 public:
  ChromeRenderViewTest();
  virtual ~ChromeRenderViewTest();

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  virtual content::ContentClient* CreateContentClient() OVERRIDE;
  virtual content::ContentBrowserClient* CreateContentBrowserClient() OVERRIDE;
  virtual content::ContentRendererClient*
      CreateContentRendererClient() OVERRIDE;

  scoped_ptr<extensions::DispatcherDelegate> extension_dispatcher_delegate_;

  autofill::TestPasswordAutofillAgent* password_autofill_;
  autofill::TestPasswordGenerationAgent* password_generation_;
  autofill::AutofillAgent* autofill_agent_;

  // Naked pointer as ownership is with content::RenderViewTest::render_thread_.
  ChromeMockRenderThread* chrome_render_thread_;
};

#endif  // CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_
