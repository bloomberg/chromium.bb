// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_TEST_MOCK_FAVICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_TEST_MOCK_FAVICON_SERVICE_H_

#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace favicon {

ACTION_TEMPLATE(PostReply,
                HAS_1_TEMPLATE_PARAMS(int, K),
                AND_1_VALUE_PARAMS(p0)) {
  auto callback = ::testing::get<K - 2>(args);
  base::CancelableTaskTracker* tracker = ::testing::get<K - 1>(args);
  return tracker->PostTask(base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
                           base::Bind(callback, p0));
}

class MockFaviconService : public FaviconService {
 public:
  MockFaviconService();
  ~MockFaviconService() override;

  MOCK_METHOD3(GetFaviconImage,
               base::CancelableTaskTracker::TaskId(
                   const GURL& icon_url,
                   const favicon_base::FaviconImageCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetRawFavicon,
               base::CancelableTaskTracker::TaskId(
                   const GURL& icon_url,
                   favicon_base::IconType icon_type,
                   int desired_size_in_pixel,
                   const favicon_base::FaviconRawBitmapCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetFavicon,
               base::CancelableTaskTracker::TaskId(
                   const GURL& icon_url,
                   favicon_base::IconType icon_type,
                   int desired_size_in_dip,
                   const favicon_base::FaviconResultsCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD3(GetFaviconImageForPageURL,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   const favicon_base::FaviconImageCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetRawFaviconForPageURL,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int icon_types,
                   int desired_size_in_pixel,
                   const favicon_base::FaviconRawBitmapCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetLargestRawFaviconForPageURL,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   const std::vector<int>& icon_types,
                   int minimum_size_in_pixels,
                   const favicon_base::FaviconRawBitmapCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD5(GetFaviconForPageURL,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int icon_types,
                   int desired_size_in_dip,
                   const favicon_base::FaviconResultsCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD6(UpdateFaviconMappingsAndFetch,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   const GURL& icon_url,
                   favicon_base::IconType icon_type,
                   int desired_size_in_dip,
                   const favicon_base::FaviconResultsCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD3(GetLargestRawFaviconForID,
               base::CancelableTaskTracker::TaskId(
                   favicon_base::FaviconID favicon_id,
                   const favicon_base::FaviconRawBitmapCallback& callback,
                   base::CancelableTaskTracker* tracker));
  MOCK_METHOD1(SetFaviconOutOfDateForPage, void(const GURL& page_url));
  MOCK_METHOD1(TouchOnDemandFavicon, void(const GURL& icon_url));
  MOCK_METHOD1(SetImportedFavicons,
               void(const favicon_base::FaviconUsageDataList& favicon_usage));
  MOCK_METHOD5(MergeFavicon,
               void(const GURL& page_url,
                    const GURL& icon_url,
                    favicon_base::IconType icon_type,
                    scoped_refptr<base::RefCountedMemory> bitmap_data,
                    const gfx::Size& pixel_size));
  MOCK_METHOD4(SetFavicons,
               void(const GURL& page_url,
                    const GURL& icon_url,
                    favicon_base::IconType icon_type,
                    const gfx::Image& image));
  MOCK_METHOD5(SetOnDemandFavicons,
               void(const GURL& page_url,
                    const GURL& icon_url,
                    favicon_base::IconType icon_type,
                    const gfx::Image& image,
                    base::Callback<void(bool)> callback));
  MOCK_METHOD1(UnableToDownloadFavicon, void(const GURL& icon_url));
  MOCK_CONST_METHOD1(WasUnableToDownloadFavicon, bool(const GURL& icon_url));
  MOCK_METHOD0(ClearUnableToDownloadFavicons, void());
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_TEST_MOCK_FAVICON_SERVICE_H_
