// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/mojo_facade.h"

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/bind_objc_block.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#import "ios/web/public/web_state/js/crw_js_injection_evaluator.h"
#include "ios/web/public/web_thread.h"
#include "mojo/public/cpp/system/core.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// Wraps an integer into |base::Value| as |TYPE_INTEGER|.
template <typename IntegerT>
std::unique_ptr<base::Value> ValueFromInteger(IntegerT handle) {
  return std::unique_ptr<base::Value>(
      new base::FundamentalValue(static_cast<int>(handle)));
}

}  // namespace

MojoFacade::MojoFacade(
    service_manager::mojom::InterfaceProvider* interface_provider,
    id<CRWJSInjectionEvaluator> script_evaluator)
    : interface_provider_(interface_provider),
      script_evaluator_(script_evaluator) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(interface_provider_);
  DCHECK(script_evaluator_);
}

MojoFacade::~MojoFacade() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
}

std::string MojoFacade::HandleMojoMessage(
    const std::string& mojo_message_as_json) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  std::string name;
  std::unique_ptr<base::DictionaryValue> args;
  GetMessageNameAndArguments(mojo_message_as_json, &name, &args);

  std::unique_ptr<base::Value> result;
  if (name == "interface_provider.getInterface") {
    result = HandleInterfaceProviderGetInterface(args.get());
  } else if (name == "core.close") {
    result = HandleCoreClose(args.get());
  } else if (name == "core.createMessagePipe") {
    result = HandleCoreCreateMessagePipe(args.get());
  } else if (name == "core.writeMessage") {
    result = HandleCoreWriteMessage(args.get());
  } else if (name == "core.readMessage") {
    result = HandleCoreReadMessage(args.get());
  } else if (name == "support.watch") {
    result = HandleSupportWatch(args.get());
  } else if (name == "support.cancelWatch") {
    result = HandleSupportCancelWatch(args.get());
  }

  if (!result) {
    return "";
  }

  std::string json_result;
  base::JSONWriter::Write(*result, &json_result);
  return json_result;
}

void MojoFacade::GetMessageNameAndArguments(
    const std::string& mojo_message_as_json,
    std::string* out_name,
    std::unique_ptr<base::DictionaryValue>* out_args) {
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> mojo_message_as_value(
      base::JSONReader::ReadAndReturnError(mojo_message_as_json, false,
                                           &error_code, &error_message));
  CHECK(!error_code);
  base::DictionaryValue* mojo_message = nullptr;
  CHECK(mojo_message_as_value->GetAsDictionary(&mojo_message));

  std::string name;
  CHECK(mojo_message->GetString("name", &name));

  base::DictionaryValue* args = nullptr;
  CHECK(mojo_message->GetDictionary("args", &args));

  *out_name = name;
  *out_args = args->CreateDeepCopy();
}

std::unique_ptr<base::Value> MojoFacade::HandleInterfaceProviderGetInterface(
    const base::DictionaryValue* args) {
  const base::Value* interface_name_as_value = nullptr;
  CHECK(args->Get("interfaceName", &interface_name_as_value));

  // By design interface_provider.getInterface either succeeds or crashes, so
  // check if interface name is a valid string is intentionally omitted.
  std::string interface_name_as_string;
  interface_name_as_value->GetAsString(&interface_name_as_string);

  mojo::MessagePipe pipe;
  interface_provider_->GetInterface(
      mojo::String::From(interface_name_as_string), std::move(pipe.handle0));

  return ValueFromInteger(pipe.handle1.release().value());
}

std::unique_ptr<base::Value> MojoFacade::HandleCoreClose(
    const base::DictionaryValue* args) {
  int handle = 0;
  CHECK(args->GetInteger("handle", &handle));

  mojo::Handle(handle).Close();

  return ValueFromInteger(MOJO_RESULT_OK);
}

std::unique_ptr<base::Value> MojoFacade::HandleCoreCreateMessagePipe(
    base::DictionaryValue* args) {
  const base::Value* options_as_value = nullptr;
  CHECK(args->Get("optionsDict", &options_as_value));

  if (options_as_value->IsType(base::Value::TYPE_DICTIONARY)) {
    // There are no options defined for CreateMessagePipe yet.
    const base::DictionaryValue* options_as_dict;
    options_as_value->GetAsDictionary(&options_as_dict);
    CHECK(options_as_dict->empty());
  }

  CHECK(options_as_value->IsType(base::Value::TYPE_NULL));

  mojo::MessagePipe message_pipe;
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  result->SetInteger("handle0", message_pipe.handle0.release().value());
  result->SetInteger("handle1", message_pipe.handle1.release().value());
  return std::unique_ptr<base::Value>(result.release());
}

std::unique_ptr<base::Value> MojoFacade::HandleCoreWriteMessage(
    base::DictionaryValue* args) {
  int handle = 0;
  CHECK(args->GetInteger("handle", &handle));

  base::ListValue* handles_list = nullptr;
  CHECK(args->GetList("handles", &handles_list));

  base::DictionaryValue* buffer = nullptr;
  CHECK(args->GetDictionary("buffer", &buffer));

  const base::Value* flags_as_value = nullptr;
  CHECK(args->Get("flags", &flags_as_value));

  int flags = MOJO_WRITE_MESSAGE_FLAG_NONE;
  if (!flags_as_value->GetAsInteger(&flags)) {
    flags = MOJO_WRITE_MESSAGE_FLAG_NONE;
  }

  std::vector<MojoHandle> handles(handles_list->GetSize());
  for (size_t i = 0; i < handles_list->GetSize(); i++) {
    int one_handle = 0;
    handles_list->GetInteger(i, &one_handle);
    handles[i] = one_handle;
  }

  std::vector<uint8_t> bytes(buffer->size());
  for (size_t i = 0; i < buffer->size(); i++) {
    int one_byte = 0;
    buffer->GetInteger(base::IntToString(i), &one_byte);
    bytes[i] = one_byte;
  }

  mojo::MessagePipeHandle message_pipe(static_cast<MojoHandle>(handle));
  MojoResult result =
      mojo::WriteMessageRaw(message_pipe, bytes.data(), bytes.size(),
                            handles.data(), handles.size(), flags);

  return ValueFromInteger(result);
}

std::unique_ptr<base::Value> MojoFacade::HandleCoreReadMessage(
    const base::DictionaryValue* args) {
  const base::Value* handle_as_value = nullptr;
  CHECK(args->Get("handle", &handle_as_value));
  int handle_as_int = 0;
  if (!handle_as_value->GetAsInteger(&handle_as_int)) {
    handle_as_int = 0;
  }

  const base::Value* flags_as_value = nullptr;
  CHECK(args->Get("flags", &flags_as_value));

  int flags = MOJO_READ_MESSAGE_FLAG_NONE;
  if (!flags_as_value->GetAsInteger(&flags)) {
    flags = MOJO_READ_MESSAGE_FLAG_NONE;
  }

  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  mojo::MessagePipeHandle handle(static_cast<MojoHandle>(handle_as_int));
  MojoResult mojo_result = mojo::ReadMessageRaw(handle, nullptr, &num_bytes,
                                                nullptr, &num_handles, flags);
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);

  if (mojo_result == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    std::vector<uint8_t> bytes(num_bytes);
    std::vector<MojoHandle> handles(num_handles);
    mojo_result = mojo::ReadMessageRaw(handle, bytes.data(), &num_bytes,
                                       handles.data(), &num_handles, flags);

    base::ListValue* handles_list = new base::ListValue;
    for (uint32_t i = 0; i < num_handles; i++) {
      handles_list->AppendInteger(handles[i]);
    }
    result->Set("handles", std::unique_ptr<base::Value>(handles_list));

    base::ListValue* buffer = new base::ListValue;
    for (uint32_t i = 0; i < num_bytes; i++) {
      buffer->AppendInteger(bytes[i]);
    }
    result->Set("buffer", std::unique_ptr<base::Value>(buffer));
  }
  result->SetInteger("result", mojo_result);

  return std::unique_ptr<base::Value>(result.release());
}

std::unique_ptr<base::Value> MojoFacade::HandleSupportWatch(
    const base::DictionaryValue* args) {
  int handle = 0;
  CHECK(args->GetInteger("handle", &handle));
  int signals = 0;
  CHECK(args->GetInteger("signals", &signals));
  int callback_id;
  CHECK(args->GetInteger("callbackId", &callback_id));

  mojo::Watcher::ReadyCallback callback = base::BindBlockArc(^(
      MojoResult result) {
    NSString* script =
        [NSString stringWithFormat:@"__crWeb.mojo.signalWatch(%d, %d)",
                                   callback_id, result];
    [script_evaluator_ executeJavaScript:script completionHandler:nil];
  });

  mojo::Watcher& watcher = watchers_[++last_watch_id_];
  watcher.Start(static_cast<mojo::Handle>(handle), signals, callback);
  return ValueFromInteger(last_watch_id_);
}

std::unique_ptr<base::Value> MojoFacade::HandleSupportCancelWatch(
    const base::DictionaryValue* args) {
  int watch_id = 0;
  CHECK(args->GetInteger("watchId", &watch_id));
  watchers_.erase(watch_id);
  return nullptr;
}

}  // namespace web
