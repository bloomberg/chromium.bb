// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_url_checker_impl.h"

#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"

namespace safe_browsing {
namespace {

// TODO(yzshen): Share such value with safe_browsing::BaseResourceThrottle.
// Maximum time in milliseconds to wait for the SafeBrowsing service reputation
// check. After this amount of time the outstanding check will be aborted, and
// the resource will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;

}  // namespace

SafeBrowsingUrlCheckerImpl::SafeBrowsingUrlCheckerImpl(
    int load_flags,
    content::ResourceType resource_type,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    const base::Callback<content::WebContents*()>& web_contents_getter)
    : load_flags_(load_flags),
      resource_type_(resource_type),
      web_contents_getter_(web_contents_getter),
      database_manager_(std::move(database_manager)),
      ui_manager_(std::move(ui_manager)),
      weak_factory_(this) {}

SafeBrowsingUrlCheckerImpl::~SafeBrowsingUrlCheckerImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == STATE_CHECKING_URL)
    database_manager_->CancelCheck(this);
}

void SafeBrowsingUrlCheckerImpl::CheckUrl(const GURL& url,
                                          CheckUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DVLOG(1) << "SafeBrowsingUrlCheckerImpl checks URL: " << url;
  urls_.push_back(url);
  callbacks_.push_back(std::move(callback));

  ProcessUrls();
}

void SafeBrowsingUrlCheckerImpl::OnCheckBrowseUrlResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  DCHECK_EQ(STATE_CHECKING_URL, state_);
  DCHECK_LT(next_index_, urls_.size());
  DCHECK_EQ(urls_[next_index_], url);

  timer_.Stop();
  if (threat_type == SB_THREAT_TYPE_SAFE) {
    state_ = STATE_NONE;
    std::move(callbacks_[next_index_]).Run(true);
    next_index_++;
    ProcessUrls();
    return;
  }

  if (load_flags_ & net::LOAD_PREFETCH) {
    // TODO(yzshen): Destroy prerender contents if necessary.

    BlockAndProcessUrls();
    return;
  }

  security_interstitials::UnsafeResource resource;
  resource.url = url;
  resource.original_url = urls_[0];
  if (urls_.size() > 1)
    resource.redirect_urls = std::vector<GURL>(urls_.begin() + 1, urls_.end());
  resource.is_subresource = resource_type_ != content::RESOURCE_TYPE_MAIN_FRAME;
  resource.is_subframe = resource_type_ == content::RESOURCE_TYPE_SUB_FRAME;
  resource.threat_type = threat_type;
  resource.threat_metadata = metadata;
  resource.callback =
      base::Bind(&SafeBrowsingUrlCheckerImpl::OnBlockingPageComplete,
                 weak_factory_.GetWeakPtr());
  resource.callback_thread = content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::IO);
  resource.web_contents_getter = web_contents_getter_;
  resource.threat_source = database_manager_->GetThreatSource();

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SafeBrowsingUrlCheckerImpl::StartDisplayingBlockingPage,
                 weak_factory_.GetWeakPtr(), ui_manager_, resource));
}

// static
void SafeBrowsingUrlCheckerImpl::StartDisplayingBlockingPage(
    const base::WeakPtr<SafeBrowsingUrlCheckerImpl>& checker,
    scoped_refptr<BaseUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  if (web_contents) {
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents) {
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
    } else {
      ui_manager->DisplayBlockingPage(resource);
      return;
    }
  }

  // Tab is gone or it's being prerendered.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SafeBrowsingUrlCheckerImpl::BlockAndProcessUrls,
                     checker));
}

void SafeBrowsingUrlCheckerImpl::OnCheckUrlTimeout() {
  database_manager_->CancelCheck(this);

  OnCheckBrowseUrlResult(urls_[next_index_], safe_browsing::SB_THREAT_TYPE_SAFE,
                         ThreatMetadata());
}

void SafeBrowsingUrlCheckerImpl::ProcessUrls() {
  DCHECK_NE(STATE_BLOCKED, state_);

  if (state_ == STATE_CHECKING_URL ||
      state_ == STATE_DISPLAYING_BLOCKING_PAGE) {
    return;
  }

  while (next_index_ < urls_.size()) {
    DCHECK_EQ(STATE_NONE, state_);
    // TODO(yzshen): Consider moving CanCheckResourceType() to the renderer
    // side. That would save some IPCs. It requires a method on the
    // SafeBrowsing mojo interface to query all supported resource types.
    if (!database_manager_->CanCheckResourceType(resource_type_) ||
        database_manager_->CheckBrowseUrl(urls_[next_index_], this)) {
      std::move(callbacks_[next_index_]).Run(true);
      next_index_++;
      continue;
    }

    state_ = STATE_CHECKING_URL;
    // Start a timer to abort the check if it takes too long.
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kCheckUrlTimeoutMs), this,
                 &SafeBrowsingUrlCheckerImpl::OnCheckUrlTimeout);

    break;
  }
}

void SafeBrowsingUrlCheckerImpl::BlockAndProcessUrls() {
  DVLOG(1) << "SafeBrowsingUrlCheckerImpl blocks URL: " << urls_[next_index_];
  state_ = STATE_BLOCKED;

  // If user decided to not proceed through a warning, mark all the remaining
  // redirects as "bad".
  for (; next_index_ < callbacks_.size(); ++next_index_)
    std::move(callbacks_[next_index_]).Run(false);
}

void SafeBrowsingUrlCheckerImpl::OnBlockingPageComplete(bool proceed) {
  DCHECK_EQ(STATE_DISPLAYING_BLOCKING_PAGE, state_);

  if (proceed) {
    state_ = STATE_NONE;
    std::move(callbacks_[next_index_]).Run(true);
    next_index_++;
    ProcessUrls();
  } else {
    BlockAndProcessUrls();
  }
}

}  // namespace safe_browsing
