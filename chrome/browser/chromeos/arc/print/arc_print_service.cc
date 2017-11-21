// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/print/arc_print_service.h"

#include <utility>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "mojo/edk/embedder/embedder.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace {

base::Optional<base::FilePath> SavePdf(base::File file) {
  base::AssertBlockingAllowed();

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

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcPrintService.
class ArcPrintServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPrintService,
          ArcPrintServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPrintServiceFactory";

  static ArcPrintServiceFactory* GetInstance() {
    return base::Singleton<ArcPrintServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPrintServiceFactory>;
  ArcPrintServiceFactory() = default;
  ~ArcPrintServiceFactory() override = default;
};

}  // namespace

// static
ArcPrintService* ArcPrintService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPrintServiceFactory::GetForBrowserContext(context);
}

ArcPrintService::ArcPrintService(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      binding_(this),
      weak_ptr_factory_(this) {
  arc_bridge_service_->print()->AddObserver(this);
}

ArcPrintService::~ArcPrintService() {
  arc_bridge_service_->print()->RemoveObserver(this);
}

void ArcPrintService::OnConnectionReady() {
  mojom::PrintInstance* print_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->print(), Init);
  DCHECK(print_instance);
  mojom::PrintHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  print_instance->Init(std::move(host_proxy));
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

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SavePdf, base::Passed(&file)),
      base::BindOnce(&ArcPrintService::OpenPdf,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcPrintService::OpenPdf(base::Optional<base::FilePath> file_path) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!file_path)
    return;

  GURL gurl = net::FilePathToFileURL(file_path.value());
  ash::Shell::Get()->shell_delegate()->OpenUrlFromArc(gurl);
  // TODO(poromov) Delete file after printing. (http://crbug.com/629843)
}

}  // namespace arc
