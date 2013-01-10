// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/file_system_natives.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "extensions/common/constants.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

namespace {

static v8::Handle<v8::Value> GetIsolatedFileSystem(
    const v8::Arguments& args) {
  DCHECK(args.Length() == 1 || args.Length() == 2);
  DCHECK(args[0]->IsString());
  std::string file_system_id(*v8::String::Utf8Value(args[0]));
  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
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

  return webframe->createFileSystem(
      WebKit::WebFileSystem::TypeIsolated,
      WebKit::WebString::fromUTF8(name),
      WebKit::WebString::fromUTF8(root));
}

}  // namespace

namespace extensions {

FileSystemNatives::FileSystemNatives()
    : ChromeV8Extension(NULL) {
  RouteStaticFunction("GetIsolatedFileSystem", &GetIsolatedFileSystem);
}

}  // namespace extensions
