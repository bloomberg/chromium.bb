// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/pepper_request_proxy.h"

#include "base/values.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "content/public/renderer/v8_value_converter.h"

namespace extensions {

PepperRequestProxy::PepperRequestProxy(ChromeV8Context* context)
    : context_(context),
      isolate_(context->v8_context()->GetIsolate()),
      next_request_id_(0) {}

PepperRequestProxy::~PepperRequestProxy() {}

bool PepperRequestProxy::StartRequest(const ResponseCallback& callback,
                                      const std::string& request_name,
                                      const base::ListValue& args,
                                      std::string* error) {
  int request_id = next_request_id_++;
  pending_request_map_[request_id] = callback;

  // TODO(sammc): Converting from base::Value to v8::Value and then back to
  // base::Value is not optimal. For most API calls the JS code doesn't do much.
  // http://crbug.com/324115.
  v8::HandleScope scope(isolate_);
  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  std::vector<v8::Handle<v8::Value> > v8_args;
  v8_args.push_back(v8::String::NewFromUtf8(isolate_, request_name.c_str()));
  v8_args.push_back(v8::Integer::New(isolate_, request_id));
  for (base::ListValue::const_iterator it = args.begin(); it != args.end();
       ++it) {
    v8_args.push_back(converter->ToV8Value(*it, context_->v8_context()));
  }
  v8::Handle<v8::Value> v8_error = context_->module_system()->CallModuleMethod(
      "pepper_request", "startRequest", &v8_args);
  if (v8_error->IsString()) {
    if (error) {
      *error = *v8::String::Utf8Value(v8_error);
      pending_request_map_.erase(request_id);
    }
    return false;
  }

  return true;
}

void PepperRequestProxy::OnResponseReceived(int request_id,
                                            bool success,
                                            const base::ListValue& args,
                                            const std::string& error) {
  PendingRequestMap::iterator it = pending_request_map_.find(request_id);
  DCHECK(it != pending_request_map_.end());
  it->second.Run(success, args, error);
  pending_request_map_.erase(it);
}

}  // namespace extensions
