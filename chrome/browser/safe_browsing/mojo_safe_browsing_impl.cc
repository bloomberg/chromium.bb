// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/mojo_safe_browsing_impl.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/load_flags.h"

namespace safe_browsing {
namespace {

// TODO(yzshen): Share such value with safe_browsing::BaseResourceThrottle.
// Maximum time in milliseconds to wait for the SafeBrowsing service reputation
// check. After this amount of time the outstanding check will be aborted, and
// the resource will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;

content::WebContents* GetWebContentsFromID(int render_process_id,
                                           int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return nullptr;

  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

// TODO(yzshen): Handle the case where SafeBrowsing is not enabled, or
// !database_manager()->IsSupported().
// TODO(yzshen): Make sure it also works on Andorid.
// TODO(yzshen): Do all the logging like what BaseResourceThrottle does.
class SafeBrowsingUrlCheckerImpl : public mojom::SafeBrowsingUrlChecker,
                                   public SafeBrowsingDatabaseManager::Client {
 public:
  SafeBrowsingUrlCheckerImpl(
      int load_flags,
      content::ResourceType resource_type,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      int render_process_id,
      int render_frame_id)
      : load_flags_(load_flags),
        resource_type_(resource_type),
        render_process_id_(render_process_id),
        render_frame_id_(render_frame_id),
        database_manager_(std::move(database_manager)),
        ui_manager_(std::move(ui_manager)),
        weak_factory_(this) {
    DCHECK_NE(resource_type, content::RESOURCE_TYPE_MAIN_FRAME);
    DCHECK_NE(resource_type, content::RESOURCE_TYPE_SUB_FRAME);
  }

  ~SafeBrowsingUrlCheckerImpl() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    if (state_ == STATE_CHECKING_URL)
      database_manager_->CancelCheck(this);

    for (size_t i = next_index_; i < callbacks_.size(); ++i)
      std::move(callbacks_[i]).Run(false);
  }

  // mojom::SafeBrowsingUrlChecker implementation.
  void CheckUrl(const GURL& url, CheckUrlCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    DVLOG(1) << "SafeBrowsingUrlCheckerImpl checks URL: " << url;
    urls_.push_back(url);
    callbacks_.push_back(std::move(callback));

    ProcessUrls();
  }

 private:
  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override {
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
      BlockAndProcessUrls();
      return;
    }

    security_interstitials::UnsafeResource resource;
    resource.url = url;
    resource.original_url = urls_[0];
    if (urls_.size() > 1)
      resource.redirect_urls =
          std::vector<GURL>(urls_.begin() + 1, urls_.end());
    resource.is_subresource = true;
    resource.is_subframe = false;
    resource.threat_type = threat_type;
    resource.threat_metadata = metadata;
    resource.callback =
        base::Bind(&SafeBrowsingUrlCheckerImpl::OnBlockingPageComplete,
                   weak_factory_.GetWeakPtr());
    resource.callback_thread = content::BrowserThread::GetTaskRunnerForThread(
        content::BrowserThread::IO);
    resource.web_contents_getter =
        base::Bind(&GetWebContentsFromID, render_process_id_, render_frame_id_);
    resource.threat_source = database_manager_->GetThreatSource();

    state_ = STATE_DISPLAYING_BLOCKING_PAGE;

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&SafeBrowsingUrlCheckerImpl::StartDisplayingBlockingPage,
                   weak_factory_.GetWeakPtr(), ui_manager_, resource));
  }

  static void StartDisplayingBlockingPage(
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

  void OnCheckUrlTimeout() {
    database_manager_->CancelCheck(this);

    OnCheckBrowseUrlResult(urls_[next_index_],
                           safe_browsing::SB_THREAT_TYPE_SAFE,
                           ThreatMetadata());
  }

  void ProcessUrls() {
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

  void BlockAndProcessUrls() {
    DVLOG(1) << "SafeBrowsingUrlCheckerImpl blocks URL: " << urls_[next_index_];
    state_ = STATE_BLOCKED;

    // If user decided to not proceed through a warning, mark all the remaining
    // redirects as "bad".
    for (; next_index_ < callbacks_.size(); ++next_index_)
      std::move(callbacks_[next_index_]).Run(false);
  }

  void OnBlockingPageComplete(bool proceed) {
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

  enum State {
    // Haven't started checking or checking is complete.
    STATE_NONE,
    // We have one outstanding URL-check.
    STATE_CHECKING_URL,
    // We're displaying a blocking page.
    STATE_DISPLAYING_BLOCKING_PAGE,
    // The blocking page has returned *not* to proceed.
    STATE_BLOCKED
  };

  const int load_flags_;
  const content::ResourceType resource_type_;
  const int render_process_id_;
  const int render_frame_id_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<BaseUIManager> ui_manager_;

  // The redirect chain for this resource, including the original URL and
  // subsequent redirect URLs.
  std::vector<GURL> urls_;
  // Callbacks corresponding to |urls_| to report check results. |urls_| and
  // |callbacks_| are always of the same size.
  std::vector<CheckUrlCallback> callbacks_;
  // |urls_| before |next_index_| have been checked. If |next_index_| is smaller
  // than the size of |urls_|, the URL at |next_index_| is being processed.
  size_t next_index_ = 0;

  State state_ = STATE_NONE;

  // Timer to abort the SafeBrowsing check if it takes too long.
  base::OneShotTimer timer_;

  base::WeakPtrFactory<SafeBrowsingUrlCheckerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUrlCheckerImpl);
};

}  // namespace

MojoSafeBrowsingImpl::MojoSafeBrowsingImpl(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    int render_process_id)
    : database_manager_(std::move(database_manager)),
      ui_manager_(std::move(ui_manager)),
      render_process_id_(render_process_id) {}

MojoSafeBrowsingImpl::~MojoSafeBrowsingImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// static
void MojoSafeBrowsingImpl::Create(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    int render_process_id,
    const service_manager::BindSourceInfo& source_info,
    mojom::SafeBrowsingRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  mojo::MakeStrongBinding(base::MakeUnique<MojoSafeBrowsingImpl>(
                              std::move(database_manager),
                              std::move(ui_manager), render_process_id),
                          std::move(request));
}

void MojoSafeBrowsingImpl::CreateCheckerAndCheck(
    int32_t render_frame_id,
    mojom::SafeBrowsingUrlCheckerRequest request,
    const GURL& url,
    int32_t load_flags,
    content::ResourceType resource_type,
    CreateCheckerAndCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto checker_impl = base::MakeUnique<SafeBrowsingUrlCheckerImpl>(
      static_cast<int>(load_flags), resource_type, database_manager_,
      ui_manager_, render_process_id_, static_cast<int>(render_frame_id));
  checker_impl->CheckUrl(url, std::move(callback));
  mojo::MakeStrongBinding(std::move(checker_impl), std::move(request));
}

}  // namespace safe_browsing
