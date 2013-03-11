// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_context.h"

#include "base/bind.h"
#include "content/browser/streams/stream_registry.h"
#include "content/public/browser/browser_context.h"

using base::UserDataAdapter;

namespace {
const char* kStreamContextKeyName = "content_stream_context";
}

namespace content {

StreamContext::StreamContext() {}

StreamContext* StreamContext::GetFor(BrowserContext* context) {
  if (!context->GetUserData(kStreamContextKeyName)) {
    scoped_refptr<StreamContext> stream = new StreamContext();
    context->SetUserData(kStreamContextKeyName,
                         new UserDataAdapter<StreamContext>(stream));
    // Check first to avoid memory leak in unittests.
    if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&StreamContext::InitializeOnIOThread, stream));
    }
  }

  return UserDataAdapter<StreamContext>::Get(context, kStreamContextKeyName);
}

void StreamContext::InitializeOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  registry_.reset(new StreamRegistry());
}

StreamContext::~StreamContext() {}

}  // namespace content
