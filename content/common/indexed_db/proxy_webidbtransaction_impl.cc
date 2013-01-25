// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/proxy_webidbtransaction_impl.h"

#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/child_thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransactionCallbacks.h"

using WebKit::WebIDBObjectStore;
using WebKit::WebIDBTransactionCallbacks;
using WebKit::WebString;

namespace content {

RendererWebIDBTransactionImpl::RendererWebIDBTransactionImpl() {
}

RendererWebIDBTransactionImpl::~RendererWebIDBTransactionImpl() {
}

void RendererWebIDBTransactionImpl::commit() {
    NOTIMPLEMENTED();
}

void RendererWebIDBTransactionImpl::abort() {
    NOTIMPLEMENTED();
}

void RendererWebIDBTransactionImpl::setCallbacks(
    WebIDBTransactionCallbacks* callbacks) {
    NOTIMPLEMENTED();
}

}  // namespace content
