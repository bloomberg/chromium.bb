// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/service/pdf_compositor_service.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/printing/service/pdf_compositor_impl.h"
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"

#if defined(OS_WIN)
#include "content/public/child/dwrite_font_proxy_init_win.h"
#endif

namespace {

void OnPdfCompositorRequest(
    const std::string& creator,
    service_manager::ServiceContextRefFactory* ref_factory,
    printing::mojom::PdfCompositorRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<printing::PdfCompositorImpl>(
                              creator, ref_factory->CreateRef()),
                          std::move(request));
}
}  // namespace

namespace printing {

PdfCompositorService::PdfCompositorService(const std::string& creator)
    : creator_(creator.empty() ? "Chromium" : creator), weak_factory_(this) {}

PdfCompositorService::~PdfCompositorService() {
#if defined(OS_WIN)
  content::UninitializeDWriteFontProxy();
#endif
}

// static
std::unique_ptr<service_manager::Service> PdfCompositorService::Create(
    const std::string& creator) {
#if defined(OS_WIN)
  // Initialize direct write font proxy so skia can use it.
  content::InitializeDWriteFontProxy();
#endif
  return base::MakeUnique<printing::PdfCompositorService>(creator);
}

void PdfCompositorService::PrepareToStart() {
  // Set up discardable memory manager.
  discardable_memory::mojom::DiscardableSharedMemoryManagerPtr manager_ptr;
  context()->connector()->BindInterface(content::mojom::kBrowserServiceName,
                                        &manager_ptr);
  discardable_shared_memory_manager_ = base::MakeUnique<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_ptr), content::UtilityThread::Get()->GetIOTaskRunner());
  DCHECK(discardable_shared_memory_manager_);
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());
}

void PdfCompositorService::OnStart() {
  PrepareToStart();

  ref_factory_ = base::MakeUnique<service_manager::ServiceContextRefFactory>(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context())));
  registry_.AddInterface(
      base::Bind(&OnPdfCompositorRequest, creator_, ref_factory_.get()));
}

void PdfCompositorService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace printing
