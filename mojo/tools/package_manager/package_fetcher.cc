// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/tools/package_manager/package_fetcher.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"

namespace mojo {

PackageFetcher::PackageFetcher(NetworkService* network_service,
                               PackageFetcherDelegate* delegate,
                               const GURL& url)
    : delegate_(delegate),
      url_(url) {
  network_service->CreateURLLoader(Get(&loader_));

  URLRequestPtr request(URLRequest::New());
  request->url = url.spec();
  request->auto_follow_redirects = true;

  loader_->Start(request.Pass(),
                 base::Bind(&PackageFetcher::OnReceivedResponse,
                            base::Unretained(this)));
}

PackageFetcher::~PackageFetcher() {
}

void PackageFetcher::OnReceivedResponse(URLResponsePtr response) {
  if (response->error) {
    printf("Got error %d (%s) for %s\n",
           response->error->code,
           response->error->description.get().c_str(),
           url_.spec().c_str());
    delegate_->FetchFailed(url_);
    return;
  }

  if (!base::CreateTemporaryFile(&output_file_name_)) {
    delegate_->FetchFailed(url_);
    return;
  }
  output_file_.Initialize(output_file_name_,
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!output_file_.IsValid()) {
    base::DeleteFile(output_file_name_, false);
    delegate_->FetchFailed(url_);
    // Danger: may be deleted now.
    return;
  }

  body_ = response->body.Pass();
  ReadData(MOJO_RESULT_OK);
  // Danger: may be deleted now.
}

void PackageFetcher::ReadData(MojoResult) {
  char buf[4096];
  uint32_t num_bytes = sizeof(buf);
  MojoResult result = ReadDataRaw(body_.get(), buf, &num_bytes,
                                  MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    WaitToReadMore();
  } else if (result == MOJO_RESULT_OK) {
    if (output_file_.WriteAtCurrentPos(buf, num_bytes) !=
        static_cast<int>(num_bytes)) {
      // Clean up the output file.
      output_file_.Close();
      base::DeleteFile(output_file_name_, false);

      delegate_->FetchFailed(url_);
      // Danger: may be deleted now.
      return;
    }
    WaitToReadMore();
  } else if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Done.
    output_file_.Close();
    delegate_->FetchSucceeded(url_, output_file_name_);
    // Danger: may be deleted now.
  }
}

void PackageFetcher::WaitToReadMore() {
  handle_watcher_.Start(
      body_.get(),
      MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_DEADLINE_INDEFINITE,
      base::Bind(&PackageFetcher::ReadData, base::Unretained(this)));
}

}  // namespace mojo
