// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_function.h"

#include "base/bind.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "chrome/browser/profiles/profile.h"

using content::BrowserThread;

namespace extensions {

AsyncApiFunction::AsyncApiFunction()
    : work_thread_id_(BrowserThread::IO) {
}

bool AsyncApiFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!PrePrepare() || !Prepare()) {
    return false;
  }
  bool rv = BrowserThread::PostTask(
      work_thread_id_, FROM_HERE,
      base::Bind(&AsyncApiFunction::WorkOnWorkThread, this));
  DCHECK(rv);
  return true;
}

bool AsyncApiFunction::PrePrepare() {
  return true;
}

void AsyncApiFunction::Work() {
}

void AsyncApiFunction::AsyncWorkStart() {
  Work();
  AsyncWorkCompleted();
}

void AsyncApiFunction::AsyncWorkCompleted() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    bool rv = BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AsyncApiFunction::RespondOnUIThread, this));
    DCHECK(rv);
  } else {
    SendResponse(Respond());
  }
}

void AsyncApiFunction::WorkOnWorkThread() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id_));
  DCHECK(work_thread_id_ != BrowserThread::UI) <<
      "You have specified that AsyncApiFunction::Work() should happen on "
      "the UI thread. This nullifies the point of this class. Either "
      "specify a different thread or derive from a different class.";
  AsyncWorkStart();
}

void AsyncApiFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(Respond());
}

int AsyncApiFunction::DeprecatedExtractSrcId(size_t argument_position) {
  scoped_ptr<DictionaryValue> options(new DictionaryValue());
  if (args_->GetSize() > argument_position) {
    DictionaryValue* temp_options = NULL;
    if (args_->GetDictionary(argument_position, &temp_options))
      options.reset(temp_options->DeepCopy());
  }

  // If we tacked on a srcId to the options object, pull it out here to provide
  // to the caller.
  int src_id = -1;
  if (options->HasKey(kSrcIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetInteger(kSrcIdKey, &src_id));
  }

  return src_id;
}

int AsyncApiFunction::ExtractSrcId(const DictionaryValue* options) {
  int src_id = -1;
  if (options) {
    if (options->HasKey(kSrcIdKey))
      EXTENSION_FUNCTION_VALIDATE(options->GetInteger(kSrcIdKey, &src_id));
  }
  return src_id;
}

ApiResourceEventNotifier* AsyncApiFunction::CreateEventNotifier(int src_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new ApiResourceEventNotifier(
      profile()->GetExtensionEventRouter(), profile(), extension_id(),
      src_id, source_url());
}

}  // namespace extensions
