// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/print/arc_print_service.h"

#include <utility>

#include "ash/common/shell_delegate.h"
#include "ash/common/wm_shell.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace {

base::Optional<base::FilePath> SavePdf(base::File file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  base::File out(file_path,
                 base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  char buf[8192];
  ssize_t bytes;
  while ((bytes = file.ReadAtCurrentPos(buf, sizeof(buf))) > 0) {
    int written = out.WriteAtCurrentPos(buf, bytes);
    if (written < 0) {
      LOG(ERROR) << "Error while saving PDF to a disk";
      return base::nullopt;
    }
  }

  return file_path;
}

void OpenPdf(base::Optional<base::FilePath> file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!file_path)
    return;

  GURL gurl = net::FilePathToFileURL(file_path.value());
  ash::WmShell::Get()->delegate()->OpenUrlFromArc(gurl);
  // TODO(poromov) Delete file after printing. (http://crbug.com/629843)
}

}  // namespace

namespace arc {

ArcPrintService::ArcPrintService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->print()->AddObserver(this);
}

ArcPrintService::~ArcPrintService() {
  arc_bridge_service()->print()->RemoveObserver(this);
}

void ArcPrintService::OnInstanceReady() {
  mojom::PrintInstance* print_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->print(), Init);
  DCHECK(print_instance);
  print_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcPrintService::Print(mojo::ScopedHandle pdf_data) {
  if (!pdf_data.is_valid()) {
    LOG(ERROR) << "handle is invalid";
    return;
  }

  mojo::edk::ScopedPlatformHandle scoped_platform_handle;
  MojoResult mojo_result = mojo::edk::PassWrappedPlatformHandle(
      pdf_data.release().value(), &scoped_platform_handle);
  if (mojo_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "PassWrappedPlatformHandle failed: " << mojo_result;
    return;
  }

  base::File file(scoped_platform_handle.release().handle);

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&SavePdf, base::Passed(&file)), base::Bind(&OpenPdf));
}

}  // namespace arc
