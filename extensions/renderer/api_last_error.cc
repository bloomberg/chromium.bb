// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_last_error.h"

#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"

namespace extensions {

namespace {

const char kLastErrorProperty[] = "lastError";

class LastErrorObject final : public gin::Wrappable<LastErrorObject> {
 public:
  explicit LastErrorObject(const std::string& error) : error_(error) {}

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    DCHECK(isolate);
    return Wrappable<LastErrorObject>::GetObjectTemplateBuilder(isolate)
        .SetProperty("message", &LastErrorObject::GetLastError);
  }

  void Reset(const std::string& error) {
    error_ = error;
    accessed_ = false;
  }

  const std::string& error() const { return error_; }
  bool accessed() const { return accessed_; }

 private:
  std::string GetLastError() {
    accessed_ = true;
    return error_;
  }

  std::string error_;
  bool accessed_ = false;

  DISALLOW_COPY_AND_ASSIGN(LastErrorObject);
};

gin::WrapperInfo LastErrorObject::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace

APILastError::APILastError(const GetParent& get_parent)
    : get_parent_(get_parent) {}
APILastError::APILastError(APILastError&& other) = default;
APILastError::~APILastError() = default;

void APILastError::SetError(v8::Local<v8::Context> context,
                            const std::string& error) {
  v8::Isolate* isolate = context->GetIsolate();
  DCHECK(isolate);
  v8::HandleScope handle_scope(isolate);

  // The various accesses/sets on an object could potentially fail if script has
  // set any crazy interceptors. For the most part, we don't care about behaving
  // perfectly in these circumstances, but we eat the exception so callers don't
  // have to worry about it. We also SetVerbose() so that developers will have a
  // clue what happened if this does arise.
  // TODO(devlin): Whether or not this needs to be verbose is debatable.
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  v8::Local<v8::Object> parent = get_parent_.Run(context);
  if (parent.IsEmpty())
    return;
  v8::Local<v8::String> key = gin::StringToSymbol(isolate, kLastErrorProperty);
  v8::Local<v8::Value> v8_error;
  if (!parent->Get(context, key).ToLocal(&v8_error))
    return;

  if (!v8_error->IsUndefined()) {
    // There may be an existing last error to overwrite.
    LastErrorObject* last_error = nullptr;
    if (!gin::Converter<LastErrorObject*>::FromV8(context->GetIsolate(),
                                                  v8_error, &last_error)) {
      // If it's not a real lastError (e.g. if a script manually set it), don't
      // do anything. We shouldn't mangle a property set by other script.
      // TODO(devlin): Or should we? If someone sets chrome.runtime.lastError,
      // it might be the right course of action to overwrite it.
      return;
    }
    last_error->Reset(error);
  } else {
    DCHECK(context->GetIsolate());
    v8::Local<v8::Value> last_error =
        gin::CreateHandle(context->GetIsolate(), new LastErrorObject(error))
            .ToV8();
    DCHECK(!last_error.IsEmpty());
    // This Set() can fail, but there's nothing to do if it does (the exception
    // will be caught by the TryCatch above).
    ignore_result(parent->Set(
        context, gin::StringToSymbol(isolate, kLastErrorProperty), last_error));
  }
}

void APILastError::ClearError(v8::Local<v8::Context> context,
                              bool report_if_unchecked) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Object> parent;
  LastErrorObject* last_error = nullptr;
  v8::Local<v8::String> key;
  {
    // See comment in SetError().
    v8::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);

    parent = get_parent_.Run(context);
    if (parent.IsEmpty())
      return;
    key = gin::StringToSymbol(isolate, kLastErrorProperty);
    v8::Local<v8::Value> error;
    if (!parent->Get(context, key).ToLocal(&error))
      return;
    if (!gin::Converter<LastErrorObject*>::FromV8(context->GetIsolate(), error,
                                                  &last_error)) {
      return;
    }
  }

  if (report_if_unchecked && !last_error->accessed()) {
    isolate->ThrowException(
        v8::Exception::Error(gin::StringToV8(isolate, last_error->error())));
  }

  // See comment in SetError().
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  // This Delete() can fail, but there's nothing to do if it does (the exception
  // will be caught by the TryCatch above).
  parent->Delete(context, key);
}

}  // namespace extensions
