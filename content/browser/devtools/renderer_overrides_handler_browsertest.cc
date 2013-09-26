// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/devtools/renderer_overrides_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"

namespace content {

class RendererOverridesHandlerTest : public ContentBrowserTest {
 public:
  scoped_ptr<base::DictionaryValue> last_message_;

  void OnMessageSent(const std::string& message) {
    JSONStringValueSerializer serializer(message);
    int error_code;
    std::string error_message;
    base::Value* root =
        serializer.Deserialize(&error_code, &error_message);
    base::DictionaryValue* root_dictionary;
    root->GetAsDictionary(&root_dictionary);
    base::DictionaryValue* result;
    root_dictionary->GetDictionary("result", &result);
    last_message_.reset(result);
    base::MessageLoop::current()->QuitNow();
  }

  bool HasValue(const std::string& path) {
    base::Value* value = 0;
    return last_message_->Get(path, &value);
  }

  bool HasUsageItem(const std::string& path_to_list,
                    const std::string& usage_item_id) {
    base::ListValue* list;
    if (!last_message_->GetList(path_to_list, &list))
      return false;

    for (size_t i = 0; i != list->GetSize(); i++) {
      base::DictionaryValue* item;
      if (!list->GetDictionary(i, &item))
        return false;
      std::string id;
      if (!item->GetString("id", &id))
        return false;
      if (id == usage_item_id)
        return true;
    }
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(RendererOverridesHandlerTest, QueryUsageAndQuota) {

  RenderViewHost* rvh = shell()->web_contents()->GetRenderViewHost();
  DevToolsAgentHost* agent_raw = DevToolsAgentHost::GetOrCreateFor(rvh).get();
  scoped_ptr<RendererOverridesHandler> handler(
      new RendererOverridesHandler(agent_raw));

  handler->SetNotifier(base::Bind(
      &RendererOverridesHandlerTest::OnMessageSent, base::Unretained(this)));

  std::string error_response;
  scoped_refptr<DevToolsProtocol::Command> command =
      DevToolsProtocol::ParseCommand("{"
              "\"id\": 1,"
              "\"method\": \"Page.queryUsageAndQuota\","
              "\"params\": {"
                  "\"securityOrigin\": \"http://example.com\""
              "}"
          "}", &error_response);
  ASSERT_TRUE(command.get());

  scoped_refptr<DevToolsProtocol::Response> response =
      handler->HandleCommand(command);

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(HasValue("quota.persistent"));
  EXPECT_TRUE(HasValue("quota.temporary"));
  EXPECT_TRUE(HasUsageItem("usage.temporary", "appcache"));
  EXPECT_TRUE(HasUsageItem("usage.temporary", "database"));
  EXPECT_TRUE(HasUsageItem("usage.temporary", "indexeddatabase"));
  EXPECT_TRUE(HasUsageItem("usage.temporary", "filesystem"));
  EXPECT_TRUE(HasUsageItem("usage.persistent", "filesystem"));
}

}  // namespace content

