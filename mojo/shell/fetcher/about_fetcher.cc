// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/fetcher/about_fetcher.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"

namespace mojo {
namespace shell {
namespace {

void RunFetcherCallback(const Fetcher::FetchCallback& callback,
                        scoped_ptr<Fetcher> fetcher,
                        bool success) {
  callback.Run(success ? std::move(fetcher) : nullptr);
}

}  // namespace

const char AboutFetcher::kAboutScheme[] = "about";
const char AboutFetcher::kAboutBlankURL[] = "about:blank";

// static
void AboutFetcher::Start(const GURL& url,
                         const FetchCallback& loader_callback) {
  // The object manages its own lifespan.
  new AboutFetcher(url, loader_callback);
}

AboutFetcher::AboutFetcher(const GURL& url,
                           const FetchCallback& loader_callback)
    : Fetcher(loader_callback), url_(url) {
  BuildResponse();
}

AboutFetcher::~AboutFetcher() {}

void AboutFetcher::BuildResponse() {
  response_ = URLResponse::New();
  response_->url = url_.spec();

  // about: URLs other than about:blank are not supported yet.
  //
  // TODO(yzshen): crbug.com/516494 Eventually we need a general solution to
  // generate error page for network errors/unrecognized app format/etc.
  response_->status_code = (url_ == GURL(kAboutBlankURL)) ? 200 : 404;

  response_->mime_type = "text/html";
  PostToRunCallback(true);
}

void AboutFetcher::PostToRunCallback(bool success) {
  // Also pass |this| on failure, so that we won't destroy the object while the
  // constructor is still on the call stack.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(RunFetcherCallback, loader_callback_,
                 base::Passed(make_scoped_ptr<Fetcher>(this)), success));
}

const GURL& AboutFetcher::GetURL() const {
  return url_;
}

GURL AboutFetcher::GetRedirectURL() const {
  return GURL::EmptyGURL();
}

GURL AboutFetcher::GetRedirectReferer() const {
  return GURL::EmptyGURL();
}

URLResponsePtr AboutFetcher::AsURLResponse(base::TaskRunner* task_runner,
                                           uint32_t skip) {
  DCHECK(response_);

  // Ignore |skip| because the only URL handled currently is "about:blank" which
  // doesn't have a body.
  DCHECK(!response_->body.is_valid());

  return std::move(response_);
}

void AboutFetcher::AsPath(
    base::TaskRunner* task_runner,
    base::Callback<void(const base::FilePath&, bool)> callback) {
  NOTIMPLEMENTED();
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, base::FilePath(), false));
}

std::string AboutFetcher::MimeType() {
  DCHECK(response_);
  return response_->mime_type;
}

bool AboutFetcher::HasMojoMagic() {
  return false;
}

bool AboutFetcher::PeekFirstLine(std::string* line) {
  // The only URL handled currently is "about:blank" which doesn't have a body.
  return false;
}

}  // namespace shell
}  // namespace mojo
