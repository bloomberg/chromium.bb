// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/font_service/font_service_app.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/platform_handle/platform_handle_functions.h"

COMPILE_ASSERT(static_cast<uint32>(SkTypeface::kNormal) ==
                   static_cast<uint32>(font_service::TYPEFACE_STYLE_NORMAL),
               typeface_flags_should_match);
COMPILE_ASSERT(static_cast<uint32>(SkTypeface::kBold) ==
                   static_cast<uint32>(font_service::TYPEFACE_STYLE_BOLD),
               typeface_flags_should_match);
COMPILE_ASSERT(static_cast<uint32>(SkTypeface::kItalic) ==
                   static_cast<uint32>(font_service::TYPEFACE_STYLE_ITALIC),
               typeface_flags_should_match);
COMPILE_ASSERT(
    static_cast<uint32>(SkTypeface::kBoldItalic) ==
        static_cast<uint32>(font_service::TYPEFACE_STYLE_BOLD_ITALIC),
    typeface_flags_should_match);

namespace {

mojo::ScopedHandle GetHandleForPath(const base::FilePath& path) {
  if (path.empty())
    return mojo::ScopedHandle();

  mojo::ScopedHandle to_pass;
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(WARNING) << "file not valid, path=" << path.value();
    return mojo::ScopedHandle();
  }

  MojoHandle mojo_handle;
  MojoResult create_result =
      MojoCreatePlatformHandleWrapper(file.TakePlatformFile(), &mojo_handle);
  if (create_result != MOJO_RESULT_OK) {
    LOG(WARNING) << "unable to create wrapper, path=" << path.value()
                 << "result=" << create_result;
    return mojo::ScopedHandle();
  }

  return mojo::ScopedHandle(mojo::Handle(mojo_handle)).Pass();
}

}  // namespace

namespace font_service {

FontServiceApp::FontServiceApp() {}

FontServiceApp::~FontServiceApp() {}

void FontServiceApp::Initialize(mojo::ApplicationImpl* app) {}

bool FontServiceApp::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void FontServiceApp::Create(mojo::ApplicationConnection* connection,
                            mojo::InterfaceRequest<FontService> request) {
  bindings_.AddBinding(this, request.Pass());
}

void FontServiceApp::MatchFamilyName(const mojo::String& family_name,
                                     TypefaceStyle requested_style,
                                     const MatchFamilyNameCallback& callback) {
  SkFontConfigInterface::FontIdentity result_identity;
  SkString result_family;
  SkTypeface::Style result_style;
  SkFontConfigInterface* fc =
      SkFontConfigInterface::GetSingletonDirectInterface();
  const bool r = fc->matchFamilyName(
      family_name.data(), static_cast<SkTypeface::Style>(requested_style),
      &result_identity, &result_family, &result_style);

  if (!r) {
    callback.Run(nullptr, "", TYPEFACE_STYLE_NORMAL);
    return;
  }

  // Stash away the returned path, so we can give it an ID (index)
  // which will later be given to us in a request to open the file.
  int index = FindOrAddPath(result_identity.fString);

  FontIdentityPtr identity(FontIdentity::New());
  identity->id = static_cast<uint32_t>(index);
  identity->ttc_index = result_identity.fTTCIndex;
  identity->str_representation = result_identity.fString.c_str();

  callback.Run(identity.Pass(), result_family.c_str(),
               static_cast<TypefaceStyle>(result_style));
}

void FontServiceApp::OpenStream(uint32_t id_number,
                                const OpenStreamCallback& callback) {
  mojo::ScopedHandle handle;
  if (id_number < static_cast<uint32_t>(paths_.count())) {
    handle =
        GetHandleForPath(base::FilePath(paths_[id_number]->c_str())).Pass();
  }

  callback.Run(handle.Pass());
}

int FontServiceApp::FindOrAddPath(const SkString& path) {
  int count = paths_.count();
  for (int i = 0; i < count; ++i) {
    if (path == *paths_[i])
      return i;
  }
  *paths_.append() = new SkString(path);
  return count;
}

}  // namespace font_service
