// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_distiller_page_factory.h"

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/reading_list/favicon_web_state_dispatcher_impl.h"
#include "ios/chrome/browser/reading_list/reading_list_distiller_page.h"
#include "ios/web/public/browser_state.h"

namespace reading_list {

ReadingListDistillerPageFactory::ReadingListDistillerPageFactory(
    web::BrowserState* browser_state)
    : browser_state_(browser_state) {
  web_state_dispatcher_ =
      base::MakeUnique<reading_list::FaviconWebStateDispatcherImpl>(
          browser_state_, -1);
}

ReadingListDistillerPageFactory::~ReadingListDistillerPageFactory() {}

std::unique_ptr<reading_list::ReadingListDistillerPage>
ReadingListDistillerPageFactory::CreateReadingListDistillerPage() const {
  return base::MakeUnique<ReadingListDistillerPage>(
      browser_state_, web_state_dispatcher_.get());
}

std::unique_ptr<dom_distiller::DistillerPage>
ReadingListDistillerPageFactory::CreateDistillerPage(
    const gfx::Size& view_size) const {
  return base::MakeUnique<ReadingListDistillerPage>(
      browser_state_, web_state_dispatcher_.get());
}

std::unique_ptr<dom_distiller::DistillerPage>
ReadingListDistillerPageFactory::CreateDistillerPageWithHandle(
    std::unique_ptr<dom_distiller::SourcePageHandle> handle) const {
  return base::MakeUnique<ReadingListDistillerPage>(
      browser_state_, web_state_dispatcher_.get());
}

}  // namespace reading_list
