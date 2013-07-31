// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/url_response_info_util.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "content/renderer/pepper/ppb_file_ref_impl.h"
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

}  // namespace

void DataFromWebURLResponse(PP_Instance pp_instance,
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
    scoped_refptr<PPB_FileRef_Impl> file_ref(
        PPB_FileRef_Impl::CreateExternal(
            pp_instance,
            base::FilePath::FromUTF16Unsafe(file_path),
            std::string()));
    data.body_as_file_ref = file_ref->GetCreateInfo();
    file_ref->GetReference();  // The returned data has one ref for the plugin.
  }

  // We post data to a callback instead of returning it here because the new
  // implementation for FileRef is asynchronous when creating the resource.
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, data));
}

}  // namespace content
