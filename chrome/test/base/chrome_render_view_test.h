// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_
#define CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_

#include <string>

#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/chrome_mock_render_thread.h"
#include "components/autofill/renderer/autofill_agent.h"
#include "content/public/test/render_view_test.h"

namespace autofill {
class AutofillAgent;
class PasswordAutofillAgent;
}

namespace extensions {
class Dispatcher;
}

class ChromeRenderViewTest : public content::RenderViewTest {
 public:
  ChromeRenderViewTest();
  virtual ~ChromeRenderViewTest();

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  chrome::ChromeContentRendererClient chrome_content_renderer_client_;
  extensions::Dispatcher* extension_dispatcher_;

  autofill::PasswordAutofillAgent* password_autofill_;
  autofill::AutofillAgent* autofill_agent_;

  // Naked pointer as ownership is with content::RenderViewTest::render_thread_.
  ChromeMockRenderThread* chrome_render_thread_;
};

#endif  // CHROME_TEST_BASE_CHROME_RENDER_VIEW_TEST_H_
