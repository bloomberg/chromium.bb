// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

class RendererOverridesHandlerTest : public ContentBrowserTest,
                                     public DevToolsClientHost {
 protected:
  void SendCommand(const std::string& method,
                   base::DictionaryValue* params) {
    EXPECT_TRUE(DevToolsManager::GetInstance()->DispatchOnInspectorBackend(this,
        DevToolsProtocol::CreateCommand(1, method, params)->Serialize()));
    base::MessageLoop::current()->Run();
  }

  bool HasValue(const std::string& path) {
    base::Value* value = 0;
    return result_->Get(path, &value);
  }

  bool HasListItem(const std::string& path_to_list,
                   const std::string& name,
                   const std::string& value) {
    base::ListValue* list;
    if (!result_->GetList(path_to_list, &list))
      return false;

    for (size_t i = 0; i != list->GetSize(); i++) {
      base::DictionaryValue* item;
      if (!list->GetDictionary(i, &item))
        return false;
      std::string id;
      if (!item->GetString(name, &id))
        return false;
      if (id == value)
        return true;
    }
    return false;
  }

  scoped_ptr<base::DictionaryValue> result_;

 private:
  virtual void SetUpOnMainThread() OVERRIDE {
    DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        DevToolsAgentHost::GetOrCreateFor(shell()->web_contents()).get(), this);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    DevToolsManager::GetInstance()->ClientHostClosing(this);
  }

  virtual void DispatchOnInspectorFrontend(
      const std::string& message) OVERRIDE {
    scoped_ptr<base::DictionaryValue> root(
        static_cast<base::DictionaryValue*>(base::JSONReader::Read(message)));
    base::DictionaryValue* result;
    EXPECT_TRUE(root->GetDictionary("result", &result));
    result_.reset(result->DeepCopy());
    base::MessageLoop::current()->QuitNow();
  }

  virtual void InspectedContentsClosing() OVERRIDE {
    EXPECT_TRUE(false);
  }

  virtual void ReplacedWithAnotherClient() OVERRIDE {
    EXPECT_TRUE(false);
  }
};

IN_PROC_BROWSER_TEST_F(RendererOverridesHandlerTest, QueryUsageAndQuota) {
  base::DictionaryValue* params = new base::DictionaryValue();
  params->SetString("securityOrigin", "http://example.com");
  SendCommand("Page.queryUsageAndQuota", params);

  EXPECT_TRUE(HasValue("quota.persistent"));
  EXPECT_TRUE(HasValue("quota.temporary"));
  EXPECT_TRUE(HasListItem("usage.temporary", "id", "appcache"));
  EXPECT_TRUE(HasListItem("usage.temporary", "id", "database"));
  EXPECT_TRUE(HasListItem("usage.temporary", "id", "indexeddatabase"));
  EXPECT_TRUE(HasListItem("usage.temporary", "id", "filesystem"));
  EXPECT_TRUE(HasListItem("usage.persistent", "id", "filesystem"));
}

class CaptureScreenshotTest : public RendererOverridesHandlerTest {
 private:
#if !defined(OS_ANDROID)
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
#endif
};

// Does not link on Android
#if defined(OS_ANDROID)
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#else
#define MAYBE_CaptureScreenshot CaptureScreenshot
#endif
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest, MAYBE_CaptureScreenshot) {
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(content::ExecuteScript(
      shell()->web_contents()->GetRenderViewHost(),
      "document.body.style.background = '#123456'"));
  SendCommand("Page.captureScreenshot", new base::DictionaryValue());
  std::string base64;
  EXPECT_TRUE(result_->GetString("data", &base64));
  std::string png;
  EXPECT_TRUE(base::Base64Decode(base64, &png));
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(png.data()),
                        png.size(), &bitmap);
  SkColor color(bitmap.getColor(0, 0));
  EXPECT_TRUE(std::abs(0x12-(int)SkColorGetR(color)) <= 1);
  EXPECT_TRUE(std::abs(0x34-(int)SkColorGetG(color)) <= 1);
  EXPECT_TRUE(std::abs(0x56-(int)SkColorGetB(color)) <= 1);
}

}  // namespace content
