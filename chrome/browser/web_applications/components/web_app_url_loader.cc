// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_url_loader.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace web_app {

constexpr base::TimeDelta WebAppUrlLoader::kSecondsToWaitForWebContentsLoad;

namespace {

class LoaderTask : public content::WebContentsObserver {
 public:
  LoaderTask() = default;
  ~LoaderTask() override = default;

  void LoadUrl(const GURL& url,
               content::WebContents* web_contents,
               WebAppUrlLoader::ResultCallback callback) {
    url_ = url;
    callback_ = std::move(callback);
    Observe(web_contents);

    content::NavigationController::LoadURLParams load_params(url);
    load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;

    web_contents->GetController().LoadURLWithParams(load_params);

    timer_.Start(FROM_HERE, WebAppUrlLoader::kSecondsToWaitForWebContentsLoad,
                 base::BindOnce(&LoaderTask::OnLoadUrlTimeout,
                                // OneShotTimer is owned by this class and
                                // it guarantees that it will never run after
                                // it's destroyed.
                                base::Unretained(this)));
  }

  // WebContentsObserver
  // DidFinishLoad doesn't always get called after the page has fully loaded.
  // TODO(ortuno): Use DidStopLoading instead.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    // Ignore subframe loads.
    if (web_contents()->GetMainFrame() != render_frame_host) {
      return;
    }

    timer_.Stop();

    if (validated_url != url_) {
      LOG(ERROR) << "Error loading " << url_;
      LOG(ERROR) << "  page redirected to " << validated_url;
      PostResultTask(WebAppUrlLoader::Result::kRedirectedUrlLoaded);
      return;
    }
    PostResultTask(WebAppUrlLoader::Result::kUrlLoaded);
  }

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override {
    // Ignore subframe loads.
    if (web_contents()->GetMainFrame() != render_frame_host) {
      return;
    }

    timer_.Stop();

    LOG(ERROR) << "Error loading " << url_ << "  page failed to load.";
    PostResultTask(WebAppUrlLoader::Result::kFailedUnknownReason);
  }

  void WebContentsDestroyed() override {
    timer_.Stop();
    PostResultTask(WebAppUrlLoader::Result::kFailedWebContentsDestroyed);
  }

 private:
  void OnLoadUrlTimeout() {
    web_contents()->Stop();
    LOG(ERROR) << "Error loading " << url_ << " page took too long to load.";
    PostResultTask(WebAppUrlLoader::Result::kFailedPageTookTooLong);
  }

  void PostResultTask(WebAppUrlLoader::Result result) {
    Observe(nullptr);
    // Post a task to avoid reentrancy issues e.g. adding a WebContentsObserver
    // while a previous observer call is being executed.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), result));
  }

  WebAppUrlLoader::ResultCallback callback_;
  GURL url_;
  base::OneShotTimer timer_;

  base::WeakPtrFactory<LoaderTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoaderTask);
};

}  // namespace

WebAppUrlLoader::WebAppUrlLoader() = default;

WebAppUrlLoader::~WebAppUrlLoader() = default;

void WebAppUrlLoader::LoadUrl(const GURL& url,
                              content::WebContents* web_contents,
                              ResultCallback callback) {
  auto loader_task = std::make_unique<LoaderTask>();
  auto* loader_task_ptr = loader_task.get();
  loader_task_ptr->LoadUrl(
      url, web_contents,
      base::BindOnce(
          [](ResultCallback callback, std::unique_ptr<LoaderTask> task,
             Result result) {
            std::move(callback).Run(result);
            task.reset();
          },
          std::move(callback), std::move(loader_task)));
}

}  // namespace web_app
