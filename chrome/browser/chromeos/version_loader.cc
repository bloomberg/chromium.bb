// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/version_loader.h"

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"

namespace chromeos {

// Beginning of line we look for that gives version number.
static const char kPrefix[] = "CHROMEOS_RELEASE_DESCRIPTION=";

// File to look for version number in.
static const char kPath[] = "/etc/lsb-release";

VersionLoader::VersionLoader() : backend_(new Backend()) {
}

VersionLoader::Handle VersionLoader::GetVersion(
    CancelableRequestConsumerBase* consumer,
    VersionLoader::GetVersionCallback* callback) {
  if (!g_browser_process->file_thread()) {
    // This should only happen if Chrome is shutting down, so we don't do
    // anything.
    return 0;
  }

  scoped_refptr<CancelableRequest<GetVersionCallback> > request(
      new CancelableRequest<GetVersionCallback>(callback));
  AddRequest(request, consumer);

  g_browser_process->file_thread()->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(backend_.get(), &Backend::GetVersion, request));
  return request->handle();
}

// static
std::string VersionLoader::ParseVersion(const std::string& contents) {
  // The file contains lines such as:
  // XXX=YYY
  // AAA=ZZZ
  // Split the lines and look for the one that starts with kPrefix. The version
  // file is small, which is why we don't try and be tricky.
  std::vector<std::string> lines;
  SplitString(contents, '\n', &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (StartsWithASCII(lines[i], kPrefix, false)) {
      std::string version = lines[i].substr(std::string(kPrefix).size());
      if (version.size() > 1 && version[0] == '"' &&
          version[version.size() - 1] == '"') {
        // Trim trailing and leading quotes.
        version = version.substr(1, version.size() - 2);
      }
      return version;
    }
  }
  return std::string();
}

void VersionLoader::Backend::GetVersion(
    scoped_refptr<GetVersionRequest> request) {
  if (request->canceled())
    return;

  std::string version;
  std::string contents;
  if (file_util::ReadFileToString(FilePath(kPath), &contents))
    version = ParseVersion(contents);
  request->ForwardResult(GetVersionCallback::TupleType(request->handle(),
                                                       version));
}

}  // namespace chromeos
