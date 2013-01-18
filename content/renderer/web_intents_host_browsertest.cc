// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "content/common/intents_messages.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/web_intents_host.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeliveredIntentClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "webkit/glue/web_intent_data.h"

namespace content {

class WebIntentsHostTest : public RenderViewTest {
 protected:
  RenderViewImpl* view() { return static_cast<RenderViewImpl*>(view_); }
  WebIntentsHost* web_intents_host() { return view()->intents_host_; }

  void SetIntentData(const webkit_glue::WebIntentData& data) {
    web_intents_host()->OnSetIntent(data);
  }

  base::DictionaryValue* ParseValueFromReply(const IPC::Message* reply_msg) {
    IntentsHostMsg_WebIntentReply::Param reply_params;
    IntentsHostMsg_WebIntentReply::Read(reply_msg, &reply_params);
    WebKit::WebSerializedScriptValue serialized_data =
        WebKit::WebSerializedScriptValue::fromString(reply_params.a.data);

    v8::HandleScope scope;
    v8::Local<v8::Context> ctx = GetMainFrame()->mainWorldScriptContext();
    v8::Context::Scope cscope(ctx);
    v8::Handle<v8::Value> v8_val = serialized_data.deserialize();
    scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    base::Value* reply = converter->FromV8Value(v8_val, ctx);
    EXPECT_TRUE(reply->IsType(base::Value::TYPE_DICTIONARY));
    base::DictionaryValue* dict = NULL;
    reply->GetAsDictionary(&dict);
    return dict;
  }
};

TEST_F(WebIntentsHostTest, TestUnserialized) {
  webkit_glue::WebIntentData data(ASCIIToUTF16("action"), ASCIIToUTF16("type"),
                                  ASCIIToUTF16("unserialized data"));
  SetIntentData(data);

  LoadHTML("<html>"
           "<head><script>\n"
           "var d = {};\n"
           "d.action = window.webkitIntent.action;\n"
           "d.type = window.webkitIntent.type;\n"
           "d.data = window.webkitIntent.data;\n"
           "window.webkitIntent.postResult(d);\n"
           "</script></head>"
           "<body>body</body></html>");

  const IPC::Message* reply_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          IntentsHostMsg_WebIntentReply::ID);
  ASSERT_TRUE(reply_msg);
  scoped_ptr<base::DictionaryValue> dict(ParseValueFromReply(reply_msg));

  std::string reply_action;
  dict->GetStringASCII(std::string("action"), &reply_action);
  EXPECT_EQ("action", reply_action);
  std::string reply_type;
  dict->GetStringASCII(std::string("type"), &reply_type);
  EXPECT_EQ("type", reply_type);
  std::string reply_data;
  dict->GetStringASCII(std::string("data"), &reply_data);
  EXPECT_EQ("unserialized data", reply_data);
}

TEST_F(WebIntentsHostTest, TestVector) {
  webkit_glue::WebIntentData data(ASCIIToUTF16("action"), ASCIIToUTF16("type"));
  DictionaryValue* d1 = new DictionaryValue;
  d1->SetString(std::string("key1"), std::string("val1"));
  data.mime_data.Append(d1);
  DictionaryValue* d2 = new DictionaryValue;
  d2->SetString(std::string("key2"), std::string("val2"));
  data.mime_data.Append(d2);
  SetIntentData(data);

  LoadHTML("<html>"
           "<head><script>\n"
           "var d = {};\n"
           "d.action = window.webkitIntent.action;\n"
           "d.type = window.webkitIntent.type;\n"
           "d.data = window.webkitIntent.data;\n"
           "window.webkitIntent.postResult(d);\n"
           "</script></head>"
           "<body>body</body></html>");

  const IPC::Message* reply_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          IntentsHostMsg_WebIntentReply::ID);
  ASSERT_TRUE(reply_msg);
  scoped_ptr<base::DictionaryValue> dict(ParseValueFromReply(reply_msg));

  base::ListValue* payload;
  ASSERT_TRUE(dict->GetList("data", &payload));
  EXPECT_EQ(2U, payload->GetSize());

  base::DictionaryValue* v1 = NULL;
  ASSERT_TRUE(payload->GetDictionary(0, &v1));
  DCHECK(v1);
  std::string val1;
  ASSERT_TRUE(v1->GetStringASCII(std::string("key1"), &val1));
  EXPECT_EQ("val1", val1);

  base::DictionaryValue* v2 = NULL;
  ASSERT_TRUE(payload->GetDictionary(1, &v2));
  std::string val2;
  v2->GetStringASCII(std::string("key2"), &val2);
  EXPECT_EQ("val2", val2);
}

}  // namespace content
