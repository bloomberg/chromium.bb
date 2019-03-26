// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_content_renderer_overlay_manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/content_settings_renderer.mojom.h"
#include "chrome/common/prerender.mojom.h"
#include "chrome/common/search.mojom.h"
#include "components/autofill/content/common/autofill_agent.mojom.h"
#include "components/dom_distiller/content/common/distiller_page_notifier_service.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "components/subresource_filter/content/mojom/subresource_filter_agent.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"

#if defined(OS_ANDROID)
#include "chrome/common/sandbox_status_extension_android.mojom.h"
#include "third_party/blink/public/mojom/document_metadata/copyless_paste.mojom.h"
#endif

#if defined(OS_MACOSX)
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/mojo/app_window.mojom.h"
#include "extensions/common/mojo/guest_view.mojom.h"
#endif

const service_manager::Manifest& GetChromeContentRendererOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .ExposeCapability("browser",
                          service_manager::Manifest::InterfaceList<
                              chrome::mojom::SearchBouncer,
                              heap_profiling::mojom::ProfilingClient>())
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "browser",
            service_manager::Manifest::InterfaceList<
#if defined(OS_ANDROID)
                blink::mojom::document_metadata::CopylessPaste,
                chrome::mojom::SandboxStatusExtension,
#endif
                autofill::mojom::AutofillAgent,
                autofill::mojom::PasswordAutofillAgent,
                autofill::mojom::PasswordGenerationAgent,
                blink::mojom::DisplayCutoutClient,
                blink::mojom::PauseSubresourceLoadingHandle,
                blink::mojom::PreviewsResourceLoadingHintsReceiver,
                chrome::mojom::ChromeRenderFrame,
                chrome::mojom::ContentSettingsRenderer,
                chrome::mojom::PrerenderDispatcher,
                dom_distiller::mojom::DistillerPageNotifierService,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                extensions::mojom::AppWindow,
                extensions::mojom::MimeHandlerViewContainerManager,
#endif  // TODO: need gated include back
                safe_browsing::mojom::ThreatReporter,
#if defined(FULL_SAFE_BROWSING)
                safe_browsing::mojom::PhishingDetector,
#endif
#if defined(OS_MACOSX)
                spellcheck::mojom::SpellCheckPanel,
#endif
                subresource_filter::mojom::SubresourceFilterAgent>())
        .RequireInterfaceFilterCapability_Deprecated(
            "content_browser", "navigation:frame", "multidevice_setup")
        .Build()
  };
  return *manifest;
}
