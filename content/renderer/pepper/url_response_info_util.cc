// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/url_response_info_util.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "ipc/ipc_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/url_response_info_data.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

using WebKit::WebHTTPHeaderVisitor;
using WebKit::WebString;
using WebKit::WebURLResponse;

namespace content {

namespace {

class HeaderFlattener : public WebHTTPHeaderVisitor {
 public:
  const std::string& buffer() const { return buffer_; }

  virtual void visitHeader(const WebString& name, const WebString& value) {
    if (!buffer_.empty())
      buffer_.append("\n");
    buffer_.append(name.utf8());
    buffer_.append(": ");
    buffer_.append(value.utf8());
  }

 private:
  std::string buffer_;
};

bool IsRedirect(int32_t status) {
  return status >= 300 && status <= 399;
}

void DidCreateResourceHost(const ppapi::URLResponseInfoData& in_data,
                           const base::FilePath& external_path,
                           const DataFromWebURLResponseCallback& callback,
                           int pending_resource_id) {
  ppapi::URLResponseInfoData data = in_data;
  data.body_as_file_ref = ppapi::MakeExternalFileRefCreateInfo(
      external_path, std::string(), pending_resource_id);
  callback.Run(data);
}

}  // namespace

void DataFromWebURLResponse(RendererPpapiHostImpl* host_impl,
                            PP_Instance pp_instance,
                            const WebURLResponse& response,
                            const DataFromWebURLResponseCallback& callback) {
  ppapi::URLResponseInfoData data;
  data.url = response.url().spec();
  data.status_code = response.httpStatusCode();
  data.status_text = response.httpStatusText().utf8();
  if (IsRedirect(data.status_code)) {
    data.redirect_url = response.httpHeaderField(
        WebString::fromUTF8("Location")).utf8();
  }

  HeaderFlattener flattener;
  response.visitHTTPHeaderFields(&flattener);
  data.headers = flattener.buffer();

  WebString file_path = response.downloadFilePath();
  if (!file_path.isEmpty()) {
    base::FilePath external_path = base::FilePath::FromUTF16Unsafe(file_path);
    host_impl->CreateBrowserResourceHost(
        pp_instance,
        PpapiHostMsg_FileRef_CreateExternal(external_path),
        base::Bind(&DidCreateResourceHost, data, external_path, callback));
  } else {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, data));
  }
}

}  // namespace content
