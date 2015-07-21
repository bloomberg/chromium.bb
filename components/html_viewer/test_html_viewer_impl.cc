// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/test_html_viewer_impl.h"

#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "gin/converter.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptExecutionCallback.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

using blink::WebDocument;
using blink::WebFrame;

namespace html_viewer {

namespace {

std::string V8ValueToJSONString(
    blink::WebLocalFrame* local_frame,
    const blink::WebVector<v8::Local<v8::Value>>& result) {
  // TODO(sky): use V8ValueConverter when refactored to a common place.
  DCHECK(!result.isEmpty());
  base::ListValue list_value;
  for (auto& value : result)
    list_value.Append(new base::StringValue(gin::V8ToString(value)));
  std::string json_string;
  return base::JSONWriter::Write(list_value, &json_string) ? json_string
                                                           : std::string();
}

}  // namespace

class TestHTMLViewerImpl::ExecutionCallbackImpl
    : public blink::WebScriptExecutionCallback {
 public:
  ExecutionCallbackImpl(TestHTMLViewerImpl* host,
                        const mojo::Callback<void(mojo::String)>& callback)
      : host_(host), callback_(callback) {}
  virtual ~ExecutionCallbackImpl() {}

 private:
  // blink::WebScriptExecutionCallback:
  virtual void completed(const blink::WebVector<v8::Local<v8::Value>>& result) {
    mojo::String callback_result;
    if (!result.isEmpty())
      callback_result = V8ValueToJSONString(host_->web_frame_, result);
    callback_.Run(callback_result);
    host_->CallbackCompleted(this);
  }

  TestHTMLViewerImpl* host_;
  const mojo::Callback<void(mojo::String)> callback_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionCallbackImpl);
};

TestHTMLViewerImpl::TestHTMLViewerImpl(
    blink::WebLocalFrame* web_frame,
    mojo::InterfaceRequest<TestHTMLViewer> request)
    : web_frame_(web_frame), binding_(this, request.Pass()) {}

TestHTMLViewerImpl::~TestHTMLViewerImpl() {
  STLDeleteElements(&callbacks_);
}

void TestHTMLViewerImpl::CallbackCompleted(
    ExecutionCallbackImpl* callback_impl) {
  scoped_ptr<ExecutionCallbackImpl> owned_callback_impl(callback_impl);
  callbacks_.erase(callback_impl);
}

void TestHTMLViewerImpl::GetContentAsText(
    const GetContentAsTextCallback& callback) {
  callback.Run(web_frame_->document().contentAsTextForTesting().utf8());
}

void TestHTMLViewerImpl::ExecuteScript(const mojo::String& script,
                                       const ExecuteScriptCallback& callback) {
  ExecutionCallbackImpl* callback_impl =
      new ExecutionCallbackImpl(this, callback);
  callbacks_.insert(callback_impl);
  web_frame_->requestExecuteScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString::fromUTF8(script)), false,
      callback_impl);
}

}  // namespace html_viewer
