// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/file_system_natives.h"

#include <string>

#include "extensions/common/constants.h"
#include "extensions/renderer/script_context.h"
#include "storage/common/fileapi/file_system_types.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDOMFileSystem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/origin.h"

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
}

void FileSystemNatives::GetIsolatedFileSystem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 || args.Length() == 2);
  CHECK(args[0]->IsString());
  std::string file_system_id(*v8::String::Utf8Value(args[0]));
  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::FrameForContext(context()->v8_context());
  DCHECK(webframe);

  GURL context_url =
      extensions::ScriptContext::GetDocumentLoaderURLForFrame(webframe);
  CHECK(context_url.SchemeIs(extensions::kExtensionScheme));

  const GURL origin(url::Origin::Create(context_url).Serialize());
  std::string name(storage::GetIsolatedFileSystemName(origin, file_system_id));

  // The optional second argument is the subfolder within the isolated file
  // system at which to root the DOMFileSystem we're returning to the caller.
  std::string optional_root_name;
  if (args.Length() == 2) {
    CHECK(args[1]->IsString());
    optional_root_name = *v8::String::Utf8Value(args[1]);
  }

  GURL root_url(storage::GetIsolatedFileSystemRootURIString(
      origin, file_system_id, optional_root_name));

  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::Create(
          webframe, blink::kWebFileSystemTypeIsolated,
          blink::WebString::FromUTF8(name), root_url)
          .ToV8Value(context()->v8_context()->Global(), args.GetIsolate()));
}

void FileSystemNatives::GetFileEntry(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(5, args.Length());
  CHECK(args[0]->IsString());
  std::string type_string = *v8::String::Utf8Value(args[0]);
  blink::WebFileSystemType type;
  bool is_valid_type = storage::GetFileSystemPublicType(type_string, &type);
  DCHECK(is_valid_type);
  if (is_valid_type == false) {
    return;
  }

  CHECK(args[1]->IsString());
  CHECK(args[2]->IsString());
  CHECK(args[3]->IsString());
  std::string file_system_name(*v8::String::Utf8Value(args[1]));
  GURL file_system_root_url(*v8::String::Utf8Value(args[2]));
  std::string file_path_string(*v8::String::Utf8Value(args[3]));
  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(file_path_string);
  DCHECK(storage::VirtualPath::IsAbsolute(file_path.value()));

  CHECK(args[4]->IsBoolean());
  blink::WebDOMFileSystem::EntryType entry_type =
      args[4]->BooleanValue() ? blink::WebDOMFileSystem::kEntryTypeDirectory
                              : blink::WebDOMFileSystem::kEntryTypeFile;

  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::FrameForContext(context()->v8_context());
  DCHECK(webframe);
  args.GetReturnValue().Set(
      blink::WebDOMFileSystem::Create(
          webframe, type, blink::WebString::FromUTF8(file_system_name),
          file_system_root_url)
          .CreateV8Entry(blink::WebString::FromUTF8(file_path_string),
                         entry_type, context()->v8_context()->Global(),
                         args.GetIsolate()));
}

void FileSystemNatives::CrackIsolatedFileSystemName(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(args.Length(), 1);
  DCHECK(args[0]->IsString());
  std::string filesystem_name = *v8::String::Utf8Value(args[0]);
  std::string filesystem_id;
  if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id))
    return;

  args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(),
                                                    filesystem_id.c_str(),
                                                    v8::String::kNormalString,
                                                    filesystem_id.size()));
}

}  // namespace extensions
