// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/print_spooler/print_session_impl.h"

#include <utility>

#include "ash/public/cpp/arc_custom_tab.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/filename_util.h"
#include "ui/aura/window.h"

namespace arc {

// static
arc::mojom::PrintSessionHostPtr PrintSessionImpl::Create(
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ash::ArcCustomTab> custom_tab,
    arc::mojom::PrintSessionInstancePtr instance) {
  if (!custom_tab || !instance)
    return nullptr;

  // This object will be deleted when the mojo connection is closed.
  arc::mojom::PrintSessionHostPtr ptr;
  new PrintSessionImpl(std::move(web_contents), std::move(custom_tab),
                       std::move(instance), mojo::MakeRequest(&ptr));
  return ptr;
}

PrintSessionImpl::PrintSessionImpl(
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ash::ArcCustomTab> custom_tab,
    arc::mojom::PrintSessionInstancePtr instance,
    arc::mojom::PrintSessionHostRequest request)
    : ArcCustomTabModalDialogHost(std::move(custom_tab),
                                  std::move(web_contents)),
      binding_(this, std::move(request)),
      instance_(std::move(instance)) {
  binding_.set_connection_error_handler(
      base::BindOnce(&PrintSessionImpl::Close, weak_ptr_factory_.GetWeakPtr()));
  web_contents_->SetUserData(UserDataKey(), base::WrapUnique(this));

  aura::Window* window = web_contents_->GetNativeView();
  custom_tab_->Attach(window);
  window->Show();

  // TODO(jschettler): Handle this correctly once crbug.com/636642 is
  // resolved. Until then, give the PDF plugin time to load.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrintSessionImpl::StartPrintAfterDelay,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

PrintSessionImpl::~PrintSessionImpl() {
  // Delete the saved print document now that it's no longer needed.
  base::FilePath file_path;
  if (!net::FileURLToFilePath(web_contents_->GetVisibleURL(), &file_path)) {
    LOG(ERROR) << "Failed to obtain file path from URL.";
    return;
  }

  if (!base::DeleteFile(file_path, false))
    LOG(ERROR) << "Failed to delete print document.";
}

void PrintSessionImpl::Close() {
  web_contents_->RemoveUserData(UserDataKey());
}

void PrintSessionImpl::OnPrintPreviewClosed() {
  instance_->OnPrintPreviewClosed();
}

void PrintSessionImpl::StartPrintAfterDelay() {
  printing::StartPrint(web_contents_.get(), false, false);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintSessionImpl)

}  // namespace arc
