// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc_support_host.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"

namespace {
const char kAction[] = "action";
const char kActionFetchAuthCode[] = "fetchAuthCode";
const char kActionCancelAuthCode[] = "cancelAuthCode";
const char kActionCloseUI[] = "closeUI";
}  // namespace

// static
const char ArcSupportHost::kHostName[] = "com.google.arc_support";

// static
const char* const ArcSupportHost::kHostOrigin[] = {
    "chrome-extension://cnbgggchhmkkdmeppjobngjoejnihlei/"
};

// static
scoped_ptr<extensions::NativeMessageHost> ArcSupportHost::Create() {
  return scoped_ptr<NativeMessageHost>(new ArcSupportHost());
}

ArcSupportHost::ArcSupportHost() {
  arc::ArcAuthService::Get()->AddObserver(this);
}

ArcSupportHost::~ArcSupportHost() {
  arc::ArcAuthService::Get()->RemoveObserver(this);
}

void ArcSupportHost::Start(Client* client) {
  DCHECK(!client_);
  client_ = client;
}

void ArcSupportHost::OnOptInUINeedToClose() {
  if (!client_)
    return;

  base::DictionaryValue response;
  response.SetString(kAction, kActionCloseUI);
  std::string response_string;
  base::JSONWriter::Write(response, &response_string);
  client_->PostMessageFromNativeHost(response_string);
}

void ArcSupportHost::OnMessage(const std::string& request_string) {
  scoped_ptr<base::Value> request_value =
      base::JSONReader::Read(request_string);
  scoped_ptr<base::DictionaryValue> request(
      static_cast<base::DictionaryValue*>(request_value.release()));
  if (!request.get()) {
    NOTREACHED();
    return;
  }

  std::string action;
  if (!request->GetString(kAction, &action)) {
    NOTREACHED();
    return;
  }

  if (action == kActionFetchAuthCode) {
    arc::ArcAuthService::Get()->FetchAuthCode();
  } else if (action == kActionCancelAuthCode) {
    arc::ArcAuthService::Get()->CancelAuthCode();
  } else {
    NOTREACHED();
  }
}

scoped_refptr<base::SingleThreadTaskRunner> ArcSupportHost::task_runner()
    const {
  return base::ThreadTaskRunnerHandle::Get();
}
