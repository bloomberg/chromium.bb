// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_utils.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/proto/any.pb.h"
#include "components/offline_pages/core/prefetch/proto/offline_pages.pb.h"

namespace offline_pages {

const char kPageBundleTypeURL[] =
    "type.googleapis.com/chrome.offlinepages.v1.PageBundle";

bool ParsePageBundleInAnyData(const proto::Any& any_data,
                              std::vector<RenderPageInfo>* pages) {
  if (any_data.type_url() != kPageBundleTypeURL) {
    DVLOG(1) << "Wrong type url in any data";
    return false;
  }

  proto::PageBundle page_bundle;
  if (!page_bundle.ParseFromString(any_data.value())) {
    DVLOG(1) << "Failed to parse PageBundle in any data";
    return false;
  }

  if (!page_bundle.archives_size()) {
    DVLOG(1) << "No archive in PageBundle";
    return false;
  }

  for (int i = 0; i < page_bundle.archives_size(); ++i) {
    const proto::Archive& archive = page_bundle.archives(i);

    if (!archive.page_infos_size()) {
      DVLOG(1) << "No page in archive";
      return false;
      ;
    }

    // Only one page is available in PageInfos.
    const proto::PageInfo& page_info = archive.page_infos(0);

    if (page_info.url().empty()) {
      DVLOG(1) << "Empty page url";
      return false;
      ;
    }

    RenderPageInfo page;
    page.url = page_info.url();
    page.redirect_url = page_info.redirect_url();
    if (page_info.has_status()) {
      switch (page_info.status().code()) {
        case proto::OK:
          page.status = RenderStatus::RENDERED;
          break;
        case proto::NOT_FOUND:
          page.status = RenderStatus::PENDING;
          break;
        case proto::FAILED_PRECONDITION:
          page.status = RenderStatus::EXCEEDED_LIMIT;
          break;
        case proto::UNKNOWN:
          page.status = RenderStatus::FAILED;
          break;
        default:
          NOTREACHED();
          break;
      }
    } else {
      page.status = RenderStatus::RENDERED;
    }

    if (page.status == RenderStatus::RENDERED) {
      page.body_name = archive.body_name();
      page.body_length = archive.body_length();
      page.render_time =
          base::Time::FromJavaTime(page_info.render_time().seconds() * 1000 +
                                   page_info.render_time().nanos() / 1000000);
    }

    DVLOG(1) << "Got page " << page.url << " " << static_cast<int>(page.status)
             << " " << page.body_name << " " << page.body_length;
    pages->push_back(page);
  }

  return true;
}

}  // namespace offline_pages
