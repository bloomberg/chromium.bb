// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/file_system_natives.h"

#include <string>

#include "extensions/common/constants.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDOMError.h"
#include "third_party/WebKit/public/web/WebDOMFileSystem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace extensions {

FileSystemNatives::FileSystemNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetFileEntry",
      base::Bind(&FileSystemNatives::GetFileEntry, base::Unretained(this)));
  RouteFunction("GetIsolatedFileSystem",
                base::Bind(&FileSystemNatives::GetIsolatedFileSystem,
                           base::Unretained(this)));
  RouteFunction("CrackIsolatedFileSystemName",
                base::Bind(&FileSystemNatives::CrackIsolatedFileSystemName,
                           base::Unretained(this)));
  RouteFunction(
      "GetDOMError",
      base::Bind(&FileSystemNatives::GetDOMError, base::Unretained(this)));
}

void FileSystemNatives::GetIsolatedFileSystem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 1 || args.Length() == 2);
  DCHECK(args[0]->IsString());
  std::string file_system_id(*v8::String::Utf8Value(args[0]));
  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::frameForContext(context()->v8_context());
  DCHECK(webframe);

  GURL context_url =
      extensions::ScriptContext::GetDataSourceURLForFrame(webframe);
  CHECK(context_url.SchemeIs(extensions::kExtensionScheme));

  std::string name(storage::GetIsolatedFileSystemName(context_url.GetOrigin(),
                                                      file_system_id));

  // The optional second argument is the subfolder within the isolated file
  // system at which to root the DOMFileSystem we're returning to the caller.
  std::string optional_root_name;
  if (args.Length() == 2) {
    DCHECK(args[1]->IsString());
    optional_root_name = *v8::String::Utf8Value(args[1]);
  }

  GURL root_url(storage::GetIsolatedFileSystemRootURIString(
      context_url.GetOrigin(), file_system_id, optional_root_name));

  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::create(webframe,
                                      blink::WebFileSystemTypeIsolated,
                                      blink::WebString::fromUTF8(name),
                                      root_url)
          .toV8Value(args.Holder(), args.GetIsolate()));
}

void FileSystemNatives::GetFileEntry(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 5);
  DCHECK(args[0]->IsString());
  std::string type_string = *v8::String::Utf8Value(args[0]->ToString());
  blink::WebFileSystemType type;
  bool is_valid_type = storage::GetFileSystemPublicType(type_string, &type);
  DCHECK(is_valid_type);
  if (is_valid_type == false) {
    return;
  }

  DCHECK(args[1]->IsString());
  DCHECK(args[2]->IsString());
  DCHECK(args[3]->IsString());
  std::string file_system_name(*v8::String::Utf8Value(args[1]->ToString()));
  GURL file_system_root_url(*v8::String::Utf8Value(args[2]->ToString()));
  std::string file_path_string(*v8::String::Utf8Value(args[3]->ToString()));
  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(file_path_string);
  DCHECK(storage::VirtualPath::IsAbsolute(file_path.value()));

  DCHECK(args[4]->IsBoolean());
  blink::WebDOMFileSystem::EntryType entry_type =
      args[4]->BooleanValue() ? blink::WebDOMFileSystem::EntryTypeDirectory
                              : blink::WebDOMFileSystem::EntryTypeFile;

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::frameForContext(context()->v8_context());
  DCHECK(webframe);
  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::create(
          webframe,
          type,
          blink::WebString::fromUTF8(file_system_name),
          file_system_root_url)
          .createV8Entry(blink::WebString::fromUTF8(file_path_string),
                         entry_type,
                         args.Holder(),
                         args.GetIsolate()));
}

void FileSystemNatives::CrackIsolatedFileSystemName(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(args.Length(), 1);
  DCHECK(args[0]->IsString());
  std::string filesystem_name = *v8::String::Utf8Value(args[0]->ToString());
  std::string filesystem_id;
  if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id))
    return;

  args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(),
                                                    filesystem_id.c_str(),
                                                    v8::String::kNormalString,
                                                    filesystem_id.size()));
}

void FileSystemNatives::GetDOMError(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2) {
    NOTREACHED();
    return;
  }
  if (!args[0]->IsString()) {
    NOTREACHED();
    return;
  }
  if (!args[1]->IsString()) {
    NOTREACHED();
    return;
  }

  std::string name(*v8::String::Utf8Value(args[0]));
  if (name.empty()) {
    NOTREACHED();
    return;
  }
  std::string message(*v8::String::Utf8Value(args[1]));
  // message is optional hence empty is fine.

  blink::WebDOMError dom_error = blink::WebDOMError::create(
      blink::WebString::fromUTF8(name), blink::WebString::fromUTF8(message));
  args.GetReturnValue().Set(
      dom_error.toV8Value(args.Holder(), args.GetIsolate()));
}

}  // namespace extensions
