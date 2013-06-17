// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_system_natives.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "extensions/common/constants.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/platform/WebFileSystem.h"
#include "third_party/WebKit/public/platform/WebFileSystemType.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace extensions {

FileSystemNatives::FileSystemNatives(ChromeV8Context* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("GetFileEntry",
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
  DCHECK(args.Length() == 1 || args.Length() == 2);
  DCHECK(args[0]->IsString());
  std::string file_system_id(*v8::String::Utf8Value(args[0]));
  WebKit::WebFrame* webframe =
      WebKit::WebFrame::frameForContext(context()->v8_context());
  DCHECK(webframe);

  GURL context_url =
      extensions::UserScriptSlave::GetDataSourceURLForFrame(webframe);
  CHECK(context_url.SchemeIs(extensions::kExtensionScheme));

  std::string name(fileapi::GetIsolatedFileSystemName(context_url.GetOrigin(),
                                                      file_system_id));

  // The optional second argument is the subfolder within the isolated file
  // system at which to root the DOMFileSystem we're returning to the caller.
  std::string optional_root_name;
  if (args.Length() == 2) {
    DCHECK(args[1]->IsString());
    optional_root_name = *v8::String::Utf8Value(args[1]);
  }

  std::string root(fileapi::GetIsolatedFileSystemRootURIString(
      context_url.GetOrigin(),
      file_system_id,
      optional_root_name));

  args.GetReturnValue().Set(webframe->createFileSystem(
      WebKit::WebFileSystemTypeIsolated,
      WebKit::WebString::fromUTF8(name),
      WebKit::WebString::fromUTF8(root)));
}

void FileSystemNatives::GetFileEntry(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 5);
  DCHECK(args[0]->IsString());
  std::string type_string = *v8::String::Utf8Value(args[0]->ToString());
  WebKit::WebFileSystemType type;
  bool is_valid_type = fileapi::GetFileSystemPublicType(type_string, &type);
  DCHECK(is_valid_type);
  if (is_valid_type == false) {
    return;
  }

  DCHECK(args[1]->IsString());
  DCHECK(args[2]->IsString());
  DCHECK(args[3]->IsString());
  std::string file_system_name(*v8::String::Utf8Value(args[1]->ToString()));
  std::string file_system_root_url(*v8::String::Utf8Value(args[2]->ToString()));
  std::string file_path_string(*v8::String::Utf8Value(args[3]->ToString()));
  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(file_path_string);
  DCHECK(fileapi::VirtualPath::IsAbsolute(file_path.value()));

  DCHECK(args[4]->IsBoolean());
  bool is_directory = args[4]->BooleanValue();

  WebKit::WebFrame* webframe =
      WebKit::WebFrame::frameForContext(context()->v8_context());
  DCHECK(webframe);
  args.GetReturnValue().Set(webframe->createFileEntry(
      type,
      WebKit::WebString::fromUTF8(file_system_name),
      WebKit::WebString::fromUTF8(file_system_root_url),
      WebKit::WebString::fromUTF8(file_path_string),
      is_directory));
}

void FileSystemNatives::CrackIsolatedFileSystemName(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(args.Length(), 1);
  DCHECK(args[0]->IsString());
  std::string filesystem_name = *v8::String::Utf8Value(args[0]->ToString());
  std::string filesystem_id;
  if (!fileapi::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id))
    return;

  args.GetReturnValue().Set(
      v8::String::New(filesystem_id.c_str(), filesystem_id.size()));
}

}  // namespace extensions
