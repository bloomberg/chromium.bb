// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/mojo_safe_browsing_impl.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_url_checker_impl.h"
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

content::WebContents* GetWebContentsFromID(int render_process_id,
                                           int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return nullptr;

  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

// This class wraps a base::OnceCallback<void(bool)> and runs it on destruction,
// if it hasn't been run yet.
class BooleanCallbackWrapper {
 public:
  using BooleanCallback = base::OnceCallback<void(bool)>;

  explicit BooleanCallbackWrapper(BooleanCallback callback)
      : callback_(std::move(callback)) {}
  ~BooleanCallbackWrapper() {
    if (callback_)
      Run(false);
  }

  void Run(bool value) { std::move(callback_).Run(value); }

 private:
  BooleanCallback callback_;
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
      ui_manager_,
      base::Bind(&GetWebContentsFromID, render_process_id_,
                 static_cast<int>(render_frame_id)));

  checker_impl->CheckUrl(
      url, base::BindOnce(
               &BooleanCallbackWrapper::Run,
               base::Owned(new BooleanCallbackWrapper(std::move(callback)))));
  mojo::MakeStrongBinding(std::move(checker_impl), std::move(request));
}

}  // namespace safe_browsing
