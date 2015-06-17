// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/profile_utils.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/dom_distiller/lazy_dom_distiller_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "components/dom_distiller/content/distiller_javascript_utils.h"
#include "components/dom_distiller/content/dom_distiller_viewer_source.h"
#include "components/dom_distiller/core/external_feedback_reporter.h"
#include "components/dom_distiller/core/url_constants.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/dom_distiller/external_feedback_reporter_android.h"
#endif  // defined(OS_ANDROID)

void RegisterDomDistillerViewerSource(Profile* profile) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableDomDistiller)) {
    dom_distiller::DomDistillerServiceFactory* dom_distiller_service_factory =
        dom_distiller::DomDistillerServiceFactory::GetInstance();
    // The LazyDomDistillerService deletes itself when the profile is destroyed.
    dom_distiller::LazyDomDistillerService* lazy_service =
        new dom_distiller::LazyDomDistillerService(
            profile, dom_distiller_service_factory);
    scoped_ptr<dom_distiller::ExternalFeedbackReporter> reporter;

#if defined(OS_ANDROID)
    reporter.reset(
        new dom_distiller::android::ExternalFeedbackReporterAndroid());
#endif  // defined(OS_ANDROID)

    // Set the JavaScript world ID.
    if (!dom_distiller::DistillerJavaScriptWorldIdIsSet()) {
      dom_distiller::SetDistillerJavaScriptWorldId(
          chrome::ISOLATED_WORLD_ID_CHROME_INTERNAL);
    }

    content::URLDataSource::Add(
        profile,
        new dom_distiller::DomDistillerViewerSource(
            lazy_service, dom_distiller::kDomDistillerScheme, reporter.Pass()));
  }
}
