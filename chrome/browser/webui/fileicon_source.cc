// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/fileicon_source.h"

#include "base/callback.h"
#include "base/file_path.h"
#include "base/ref_counted_memory.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/gfx/codec/png_codec.h"

// The path used in internal URLs to file icon data.
static const char kFileIconPath[] = "fileicon";

FileIconSource::FileIconSource()
    : DataSource(kFileIconPath, MessageLoop::current()) {}

FileIconSource::~FileIconSource() {
  cancelable_consumer_.CancelAllRequests();
}

void FileIconSource::StartDataRequest(const std::string& path,
                                      bool is_off_the_record,
                                      int request_id) {
  IconManager* im = g_browser_process->icon_manager();

  std::string escaped_path = UnescapeURLComponent(path, UnescapeRule::SPACES);

#if defined(OS_WIN)
  // The path we receive has the wrong slashes and escaping for what we need;
  // this only appears to matter for getting icons from .exe files.
  std::replace(escaped_path.begin(), escaped_path.end(), '/', '\\');
  FilePath escaped_filepath(UTF8ToWide(escaped_path));
#elif defined(OS_POSIX)
  // The correct encoding on Linux may not actually be UTF8.
  FilePath escaped_filepath(escaped_path);
#endif
  SkBitmap* icon = im->LookupIcon(escaped_filepath, IconLoader::NORMAL);

  if (icon) {
    scoped_refptr<RefCountedBytes> icon_data(new RefCountedBytes);
    gfx::PNGCodec::EncodeBGRASkBitmap(*icon, false, &icon_data->data);

    SendResponse(request_id, icon_data);
  } else {
    // Icon was not in cache, go fetch it slowly.
    IconManager::Handle h = im->LoadIcon(escaped_filepath,
        IconLoader::NORMAL,
        &cancelable_consumer_,
        NewCallback(this, &FileIconSource::OnFileIconDataAvailable));

    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(im, h, request_id);
  }
}

std::string FileIconSource::GetMimeType(const std::string&) const {
  // Rely on image decoder inferring the correct type.
  return std::string();
}

void FileIconSource::OnFileIconDataAvailable(IconManager::Handle handle,
                                             SkBitmap* icon) {
  IconManager* im = g_browser_process->icon_manager();
  int request_id = cancelable_consumer_.GetClientData(im, handle);

  if (icon) {
    scoped_refptr<RefCountedBytes> icon_data(new RefCountedBytes);
    gfx::PNGCodec::EncodeBGRASkBitmap(*icon, false, &icon_data->data);

    SendResponse(request_id, icon_data);
  } else {
    // TODO(glen): send a dummy icon.
    SendResponse(request_id, NULL);
  }
}
