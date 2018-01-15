// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/mojo_safe_browsing_impl.h"

#include <memory>
#include <vector>

#include "components/safe_browsing/browser/safe_browsing_url_checker_impl.h"
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

// This class wraps a callback for checking URL, and runs it on destruction,
// if it hasn't been run yet.
class CheckUrlCallbackWrapper {
 public:
  using Callback =
      base::OnceCallback<void(mojom::UrlCheckNotifierRequest, bool, bool)>;

  explicit CheckUrlCallbackWrapper(Callback callback)
      : callback_(std::move(callback)) {}
  ~CheckUrlCallbackWrapper() {
    if (callback_)
      Run(nullptr, true, false);
  }

  void Run(mojom::UrlCheckNotifierRequest slow_check_notifier,
           bool proceed,
           bool showed_interstitial) {
    std::move(callback_).Run(std::move(slow_check_notifier), proceed,
                             showed_interstitial);
  }

 private:
  Callback callback_;
};

}  // namespace

MojoSafeBrowsingImpl::MojoSafeBrowsingImpl(
    scoped_refptr<UrlCheckerDelegate> delegate,
    int render_process_id)
    : delegate_(std::move(delegate)), render_process_id_(render_process_id) {
  // It is safe to bind |this| as Unretained because |bindings_| is owned by
  // |this| and will not call this callback after it is destroyed.
  bindings_.set_connection_error_handler(base::Bind(
      &MojoSafeBrowsingImpl::OnConnectionError, base::Unretained(this)));
}

MojoSafeBrowsingImpl::~MojoSafeBrowsingImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

// static
void MojoSafeBrowsingImpl::MaybeCreate(
    int render_process_id,
    const base::Callback<UrlCheckerDelegate*()>& delegate_getter,
    mojom::SafeBrowsingRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  scoped_refptr<UrlCheckerDelegate> delegate = delegate_getter.Run();

  if (!delegate || !delegate->GetDatabaseManager()->IsSupported())
    return;

  auto* impl = new MojoSafeBrowsingImpl(std::move(delegate), render_process_id);
  impl->Clone(std::move(request));
  // |impl| will be freed when there are no more pipes bound to it.
}

void MojoSafeBrowsingImpl::CreateCheckerAndCheck(
    int32_t render_frame_id,
    mojom::SafeBrowsingUrlCheckerRequest request,
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    int32_t load_flags,
    content::ResourceType resource_type,
    bool has_user_gesture,
    CreateCheckerAndCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto checker_impl = std::make_unique<SafeBrowsingUrlCheckerImpl>(
      headers, static_cast<int>(load_flags), resource_type, has_user_gesture,
      delegate_,
      base::Bind(&GetWebContentsFromID, render_process_id_,
                 static_cast<int>(render_frame_id)));

  checker_impl->CheckUrl(
      url, method,
      base::BindOnce(
          &CheckUrlCallbackWrapper::Run,
          base::Owned(new CheckUrlCallbackWrapper(std::move(callback)))));
  mojo::MakeStrongBinding(std::move(checker_impl), std::move(request));
}

void MojoSafeBrowsingImpl::Clone(mojom::SafeBrowsingRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MojoSafeBrowsingImpl::OnConnectionError() {
  if (bindings_.empty())
    delete this;
}

}  // namespace safe_browsing
