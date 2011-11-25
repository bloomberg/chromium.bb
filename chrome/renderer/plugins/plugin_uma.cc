// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/plugin_uma.h"

#include <algorithm>
#include <cstring>

#include "base/metrics/histogram.h"
#include "base/string_util.h"

namespace {

// String we will use to convert mime tyoe to plugin type.
const char kWindowsMediaPlayerType[] = "application/x-mplayer2";
const char kSilverlightTypePrefix[] = "application/x-silverlight";
const char kRealPlayerTypePrefix[] = "audio/x-pn-realaudio";
const char kJavaTypeSubstring[] = "application/x-java-applet";
const char kQuickTimeType[] = "video/quicktime";

// Arrays containing file extensions connected with specific plugins.
// The arrays must be sorted because binary search is used on them.
const char* kWindowsMediaPlayerExtensions[] = {
    ".asx"
};

const char* kRealPlayerExtensions[] = {
    ".ra",
    ".ram",
    ".rm",
    ".rmm",
    ".rmp",
    ".rpm"
};

const char* kQuickTimeExtensions[] = {
    ".moov",
    ".mov",
    ".qif",
    ".qt",
    ".qti",
    ".qtif"
};

}  // namespace.

class UMASenderImpl : public MissingPluginReporter::UMASender {
  virtual void SendPluginUMA(MissingPluginReporter::PluginType plugin_type)
      OVERRIDE;
};

void UMASenderImpl::SendPluginUMA(
    MissingPluginReporter::PluginType plugin_type) {
  UMA_HISTOGRAM_ENUMERATION("Plugin.MissingPlugins",
                            plugin_type,
                            MissingPluginReporter::OTHER);
}

// static.
MissingPluginReporter* MissingPluginReporter::GetInstance() {
  return Singleton<MissingPluginReporter>::get();
}

void MissingPluginReporter::ReportPluginMissing(
    std::string plugin_mime_type, const GURL& plugin_src) {
  PluginType plugin_type;
  // If we know plugin's mime type, we use it to determine plugin's type. Else,
  // we try to determine plugin type using plugin source's extension.
  if (!plugin_mime_type.empty()) {
    StringToLowerASCII(&plugin_mime_type);
    plugin_type = MimeTypeToPluginType(plugin_mime_type);
  } else {
    plugin_type = SrcToPluginType(plugin_src);
  }
  report_sender_->SendPluginUMA(plugin_type);
}

void MissingPluginReporter::SetUMASender(UMASender* sender) {
  report_sender_.reset(sender);
}

MissingPluginReporter::MissingPluginReporter()
    : report_sender_(new UMASenderImpl()) {
}

MissingPluginReporter::~MissingPluginReporter() {
}

// static.
bool MissingPluginReporter::CompareCStrings(const char* first,
                                           const char* second) {
  return strcmp(first, second) < 0;
}

bool MissingPluginReporter::CStringArrayContainsCString(const char** array,
                                                        size_t array_size,
                                                        const char* str) {
  return std::binary_search(array, array + array_size, str, CompareCStrings);
}

void MissingPluginReporter::ExtractFileExtension(const GURL& src,
    std::string* extension) {
  std::string extension_file_path(src.ExtractFileName());
  if (extension_file_path.empty())
    extension_file_path = src.host();

  size_t last_dot = extension_file_path.find_last_of('.');
  if (last_dot != std::string::npos) {
    *extension = extension_file_path.substr(last_dot);
  } else {
    extension->clear();
  }

  StringToLowerASCII(extension);
}

MissingPluginReporter::PluginType MissingPluginReporter::SrcToPluginType(
    const GURL& src) {
  std::string file_extension;
  ExtractFileExtension(src, &file_extension);
  if (CStringArrayContainsCString(kWindowsMediaPlayerExtensions,
                                  arraysize(kWindowsMediaPlayerExtensions),
                                  file_extension.c_str())) {
    return WINDOWS_MEDIA_PLAYER;
  }

  if (CStringArrayContainsCString(kQuickTimeExtensions,
                                  arraysize(kQuickTimeExtensions),
                                  file_extension.c_str())) {
    return QUICKTIME;
  }

  if (CStringArrayContainsCString(kRealPlayerExtensions,
                                  arraysize(kRealPlayerExtensions),
                                  file_extension.c_str())) {
    return REALPLAYER;
  }

  return OTHER;
}

MissingPluginReporter::PluginType MissingPluginReporter::MimeTypeToPluginType(
    const std::string& mime_type) {
  if (strcmp(mime_type.c_str(), kWindowsMediaPlayerType) == 0)
    return WINDOWS_MEDIA_PLAYER;

  size_t prefix_length = strlen(kSilverlightTypePrefix);
  if (strncmp(mime_type.c_str(), kSilverlightTypePrefix, prefix_length) == 0)
    return SILVERLIGHT;

  prefix_length = strlen(kRealPlayerTypePrefix);
  if (strncmp(mime_type.c_str(), kRealPlayerTypePrefix, prefix_length) == 0)
    return REALPLAYER;

  if (strstr(mime_type.c_str(), kJavaTypeSubstring))
    return JAVA;

  if (strcmp(mime_type.c_str(), kQuickTimeType) == 0)
    return QUICKTIME;

  return OTHER;
}

