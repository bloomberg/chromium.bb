// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/profile_utils.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/dom_distiller/lazy_dom_distiller_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/distiller_ui_handle.h"
#include "components/dom_distiller/content/browser/dom_distiller_viewer_source.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_distiller.h"
#endif  // defined(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

void RegisterDomDistillerViewerSource(Profile* profile) {
  bool enabled_distiller = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDomDistiller);
#if defined(ENABLE_PRINT_PREVIEW)
  if (PrintPreviewDistiller::IsEnabled())
    enabled_distiller = true;
#endif  // defined(ENABLE_PRINT_PREVIEW)

  if (enabled_distiller) {
    dom_distiller::DomDistillerServiceFactory* dom_distiller_service_factory =
        dom_distiller::DomDistillerServiceFactory::GetInstance();
    // The LazyDomDistillerService deletes itself when the profile is destroyed.
    dom_distiller::LazyDomDistillerService* lazy_service =
        new dom_distiller::LazyDomDistillerService(
            profile, dom_distiller_service_factory);
    std::unique_ptr<dom_distiller::DistillerUIHandle> ui_handle;

#if BUILDFLAG(ANDROID_JAVA_UI)
    ui_handle.reset(
        new dom_distiller::android::DistillerUIHandleAndroid());
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

    // Set the JavaScript world ID.
    if (!dom_distiller::DistillerJavaScriptWorldIdIsSet()) {
      dom_distiller::SetDistillerJavaScriptWorldId(
          chrome::ISOLATED_WORLD_ID_CHROME_INTERNAL);
    }

    content::URLDataSource::Add(
        profile, new dom_distiller::DomDistillerViewerSource(
                     lazy_service, dom_distiller::kDomDistillerScheme,
                     std::move(ui_handle)));
  }
}
