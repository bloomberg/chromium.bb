// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/profile_utils.h"

#include "base/command_line.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/dom_distiller/lazy_dom_distiller_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/dom_distiller/content/dom_distiller_viewer_source.h"
#include "components/dom_distiller/core/url_constants.h"

void RegisterDomDistillerViewerSource(Profile* profile) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableDomDistiller)) {
    dom_distiller::DomDistillerServiceFactory* dom_distiller_service_factory =
        dom_distiller::DomDistillerServiceFactory::GetInstance();
    // The LazyDomDistillerService deletes itself when the profile is destroyed.
    dom_distiller::LazyDomDistillerService* lazy_service =
        new dom_distiller::LazyDomDistillerService(
            profile, dom_distiller_service_factory);
    content::URLDataSource::Add(
        profile,
        new dom_distiller::DomDistillerViewerSource(
            lazy_service, dom_distiller::kDomDistillerScheme));
  }
}
