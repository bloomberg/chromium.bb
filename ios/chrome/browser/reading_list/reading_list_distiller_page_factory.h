// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DISTILLER_PAGE_FACTORY_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DISTILLER_PAGE_FACTORY_H_

#include <memory>

#include "components/dom_distiller/core/distiller_page.h"

namespace web {
class BrowserState;
}

namespace reading_list {
class FaviconWebStateDispatcher;
class ReadingListDistillerPage;

// ReadingListDistillerPageFactory is an iOS-specific implementation of the
// DistillerPageFactory interface allowing the creation of DistillerPage
// instances.
// These instances are configured to distille the articles of the Reading List.
class ReadingListDistillerPageFactory
    : public dom_distiller::DistillerPageFactory {
 public:
  explicit ReadingListDistillerPageFactory(web::BrowserState* browser_state);
  ~ReadingListDistillerPageFactory() override;

  // Creates a ReadingListDistillerPage.
  std::unique_ptr<reading_list::ReadingListDistillerPage>
  CreateReadingListDistillerPage() const;

  // Implementation of DistillerPageFactory:
  std::unique_ptr<dom_distiller::DistillerPage> CreateDistillerPage(
      const gfx::Size& view_size) const override;
  std::unique_ptr<dom_distiller::DistillerPage> CreateDistillerPageWithHandle(
      std::unique_ptr<dom_distiller::SourcePageHandle> handle) const override;

 private:
  web::BrowserState* browser_state_;
  std::unique_ptr<FaviconWebStateDispatcher> web_state_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListDistillerPageFactory);
};

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DISTILLER_PAGE_FACTORY_H_
