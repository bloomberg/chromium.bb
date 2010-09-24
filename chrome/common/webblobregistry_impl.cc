// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webblobregistry_impl.h"

#include "base/ref_counted.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBlobData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "webkit/blob/blob_data.h"

using WebKit::WebBlobData;
using WebKit::WebBlobStorageData;
using WebKit::WebString;
using WebKit::WebURL;

WebBlobRegistryImpl::WebBlobRegistryImpl(IPC::Message::Sender* sender)
    : sender_(sender) {
}

WebBlobRegistryImpl::~WebBlobRegistryImpl() {
}

void WebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, WebBlobData& data) {
  scoped_refptr<webkit_blob::BlobData> blob_data(
      new webkit_blob::BlobData(data));
  sender_->Send(new ViewHostMsg_RegisterBlobUrl(url, blob_data));
}

void WebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, const WebURL& src_url) {
  sender_->Send(new ViewHostMsg_RegisterBlobUrlFrom(url, src_url));
}

void WebBlobRegistryImpl::unregisterBlobURL(const WebURL& url) {
  sender_->Send(new ViewHostMsg_UnregisterBlobUrl(url));
}
