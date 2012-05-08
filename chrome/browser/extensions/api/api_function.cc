// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_function.h"

#include "base/bind.h"
#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"

using content::BrowserThread;

namespace extensions {

AsyncAPIFunction::AsyncAPIFunction()
    : work_thread_id_(BrowserThread::IO) {
}

bool AsyncAPIFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  extension_service_ = profile()->GetExtensionService();

  if (!Prepare()) {
    return false;
  }
  bool rv = BrowserThread::PostTask(
      work_thread_id_, FROM_HERE,
      base::Bind(&AsyncAPIFunction::WorkOnWorkThread, this));
  DCHECK(rv);
  return true;
}

void AsyncAPIFunction::Work() {
}

void AsyncAPIFunction::AsyncWorkStart() {
  Work();
  AsyncWorkCompleted();
}

void AsyncAPIFunction::AsyncWorkCompleted() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    bool rv = BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AsyncAPIFunction::RespondOnUIThread, this));
    DCHECK(rv);
  } else {
    SendResponse(Respond());
  }
}

void AsyncAPIFunction::WorkOnWorkThread() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id_));
  DCHECK(work_thread_id_ != BrowserThread::UI) <<
      "You have specified that AsyncAPIFunction::Work() should happen on "
      "the UI thread. This nullifies the point of this class. Either "
      "specify a different thread or derive from a different class.";
  AsyncWorkStart();
}

void AsyncAPIFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(Respond());
}

int AsyncAPIFunction::ExtractSrcId(size_t argument_position) {
  scoped_ptr<DictionaryValue> options(new DictionaryValue());
  if (args_->GetSize() > argument_position) {
    DictionaryValue* temp_options = NULL;
    if (args_->GetDictionary(argument_position, &temp_options))
      options.reset(temp_options->DeepCopy());
  }

  // If we tacked on a srcId to the options object, pull it out here to provide
  // to the Socket.
  int src_id = -1;
  if (options->HasKey(kSrcIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetInteger(kSrcIdKey, &src_id));
  }

  return src_id;
}

APIResourceEventNotifier* AsyncAPIFunction::CreateEventNotifier(int src_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new APIResourceEventNotifier(
      profile()->GetExtensionEventRouter(), profile(), extension_id(),
      src_id, source_url());
}

APIResourceController* AsyncAPIFunction::controller() {
  // ExtensionService's APIResourceController is set exactly once, long before
  // this code is reached, so it's safe to access it on any thread.
  return extension_service_->api_resource_controller();
}

}  // namespace extensions
