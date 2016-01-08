// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher/network_fetcher.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/shell/data_pipe_peek.h"
#include "mojo/shell/switches.h"

namespace mojo {
namespace shell {

NetworkFetcher::NetworkFetcher(bool disable_cache,
                               mojo::URLRequestPtr request,
                               URLLoaderFactory* url_loader_factory,
                               const FetchCallback& loader_callback)
    : Fetcher(loader_callback),
      disable_cache_(false),
      url_(request->url.To<GURL>()),
      weak_ptr_factory_(this) {
  StartNetworkRequest(std::move(request), url_loader_factory);
}

NetworkFetcher::~NetworkFetcher() {
}

const GURL& NetworkFetcher::GetURL() const {
  return url_;
}

GURL NetworkFetcher::GetRedirectURL() const {
  if (!response_)
    return GURL::EmptyGURL();

  if (response_->redirect_url.is_null())
    return GURL::EmptyGURL();

  return GURL(response_->redirect_url);
}

GURL NetworkFetcher::GetRedirectReferer() const {
  if (!response_)
    return GURL::EmptyGURL();

  if (response_->redirect_referrer.is_null())
    return GURL::EmptyGURL();

  return GURL(response_->redirect_referrer);
}
URLResponsePtr NetworkFetcher::AsURLResponse(base::TaskRunner* task_runner,
                                             uint32_t skip) {
  if (skip != 0) {
    MojoResult result = ReadDataRaw(
        response_->body.get(), nullptr, &skip,
        MOJO_READ_DATA_FLAG_ALL_OR_NONE | MOJO_READ_DATA_FLAG_DISCARD);
    DCHECK_EQ(result, MOJO_RESULT_OK);
  }
  return std::move(response_);
}

void NetworkFetcher::RecordCacheToURLMapping(const base::FilePath& path,
                                             const GURL& url) {
  // This is used to extract symbols on android.
  // TODO(eseidel): All users of this log should move to using the map file.
  VLOG(1) << "Caching mojo app " << url << " at " << path.value();

  base::FilePath temp_dir;
  base::GetTempDir(&temp_dir);
  base::ProcessId pid = base::Process::Current().Pid();
  std::string map_name = base::StringPrintf("mojo_shell.%d.maps", pid);
  base::FilePath map_path = temp_dir.AppendASCII(map_name);

  // TODO(eseidel): Paths or URLs with spaces will need quoting.
  std::string map_entry = base::StringPrintf(
      "%" PRIsFP " %s\n", path.value().c_str(), url.spec().c_str());
  // TODO(eseidel): AppendToFile is missing O_CREAT, crbug.com/450696
  if (!PathExists(map_path)) {
    base::WriteFile(map_path, map_entry.data(),
                    static_cast<int>(map_entry.length()));
  } else {
    base::AppendToFile(map_path, map_entry.data(),
                       static_cast<int>(map_entry.length()));
  }
}

// For remote debugging, GDB needs to be, a apriori, aware of the filename a
// library will be loaded from. AppIds should be be both predictable and unique,
// but any hash would work. Currently we use sha256 from crypto/secure_hash.h
bool NetworkFetcher::ComputeAppId(const base::FilePath& path,
                                  std::string* digest_string) {
  scoped_ptr<crypto::SecureHash> ctx(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open " << path.value() << " for computing AppId";
    return false;
  }
  char buf[1024];
  while (file.IsValid()) {
    int bytes_read = file.ReadAtCurrentPos(buf, sizeof(buf));
    if (bytes_read == 0)
      break;
    ctx->Update(buf, bytes_read);
  }
  if (!file.IsValid()) {
    LOG(ERROR) << "Error reading " << path.value();
    return false;
  }
  // The output is really a vector of unit8, we're cheating by using a string.
  std::string output(crypto::kSHA256Length, 0);
  ctx->Finish(string_as_array(&output), output.size());
  output = base::HexEncode(output.c_str(), output.size());
  // Using lowercase for compatiblity with sha256sum output.
  *digest_string = base::ToLowerASCII(output);
  return true;
}

bool NetworkFetcher::RenameToAppId(const GURL& url,
                                   const base::FilePath& old_path,
                                   base::FilePath* new_path) {
  std::string app_id;
  if (!ComputeAppId(old_path, &app_id))
    return false;

  // Using a hash of the url as a directory to prevent a race when the same
  // bytes are downloaded from 2 different urls. In particular, if the same
  // application is connected to twice concurrently with different query
  // parameters, the directory will be different, which will prevent the
  // collision.
  std::string dirname = base::HexEncode(
      crypto::SHA256HashString(url.spec()).data(), crypto::kSHA256Length);

  base::FilePath temp_dir;
  base::GetTempDir(&temp_dir);
  base::FilePath app_dir = temp_dir.AppendASCII(dirname);
  // The directory is leaked, because it can be reused at any time if the same
  // application is downloaded. Deleting it would be racy. This is only
  // happening when --predictable-app-filenames is used.
  bool result = base::CreateDirectoryAndGetError(app_dir, nullptr);
  DCHECK(result);
  std::string unique_name = base::StringPrintf("%s.mojo", app_id.c_str());
  *new_path = app_dir.AppendASCII(unique_name);
  return base::Move(old_path, *new_path);
}

void NetworkFetcher::CopyCompleted(
    base::Callback<void(const base::FilePath&, bool)> callback,
    bool success) {
  if (success) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kPredictableAppFilenames)) {
      // The copy completed, now move to $TMP/$APP_ID.mojo before the dlopen.
      success = false;
      base::FilePath new_path;
      if (RenameToAppId(url_, path_, &new_path)) {
        if (base::PathExists(new_path)) {
          path_ = new_path;
          success = true;
        }
      }
    }
  }

  if (success)
    RecordCacheToURLMapping(path_, url_);

  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, path_, success));
}

void NetworkFetcher::AsPath(
    base::TaskRunner* task_runner,
    base::Callback<void(const base::FilePath&, bool)> callback) {
  if (!path_.empty() || !response_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, path_, base::PathExists(path_)));
    return;
  }

  base::CreateTemporaryFile(&path_);
  common::CopyToFile(std::move(response_->body), path_, task_runner,
                     base::Bind(&NetworkFetcher::CopyCompleted,
                                weak_ptr_factory_.GetWeakPtr(), callback));
}

std::string NetworkFetcher::MimeType() {
  return response_->mime_type;
}

bool NetworkFetcher::HasMojoMagic() {
  std::string magic;
  return BlockingPeekNBytes(response_->body.get(), &magic, strlen(kMojoMagic),
                            kPeekTimeout) && magic == kMojoMagic;
}

bool NetworkFetcher::PeekFirstLine(std::string* line) {
  return BlockingPeekLine(response_->body.get(), line, kMaxShebangLength,
                          kPeekTimeout);
}

void NetworkFetcher::StartNetworkRequest(mojo::URLRequestPtr request,
                                         URLLoaderFactory* url_loader_factory) {
  TRACE_EVENT_ASYNC_BEGIN1("mojo_shell", "NetworkFetcher::NetworkRequest", this,
                           "url", request->url.To<std::string>());
  request->auto_follow_redirects = false;
  request->bypass_cache = disable_cache_;

  url_loader_factory->CreateURLLoader(GetProxy(&url_loader_));
  url_loader_->Start(std::move(request),
                     base::Bind(&NetworkFetcher::OnLoadComplete,
                                weak_ptr_factory_.GetWeakPtr()));
}

void NetworkFetcher::OnLoadComplete(URLResponsePtr response) {
  TRACE_EVENT_ASYNC_END0("mojo_shell", "NetworkFetcher::NetworkRequest", this);
  scoped_ptr<Fetcher> owner(this);
  if (response->error) {
    LOG(ERROR) << "Error (" << response->error->code << ": "
               << response->error->description << ") while fetching "
               << response->url;
    loader_callback_.Run(nullptr);
    return;
  }

  response_ = std::move(response);
  loader_callback_.Run(std::move(owner));
}

}  // namespace shell
}  // namespace mojo
