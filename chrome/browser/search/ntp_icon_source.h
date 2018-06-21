// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_ICON_SOURCE_H_
#define CHROME_BROWSER_SEARCH_NTP_ICON_SOURCE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "content/public/browser/url_data_source.h"

class GURL;
class Profile;

namespace favicon_base {
struct FaviconRawBitmapResult;
}

// NTP Icon Source is the gateway between network-level chrome: requests for
// NTP icons and the various backends that may serve them.
class NtpIconSource : public content::URLDataSource {
 public:
  explicit NtpIconSource(Profile* profile);
  ~NtpIconSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool AllowCaching() const override;
  bool ShouldServiceRequest(const GURL& url,
                            content::ResourceContext* resource_context,
                            int render_process_id) const override;

 private:
  struct NtpIconRequest;
  void OnFaviconDataAvailable(
      const NtpIconRequest& request,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  base::CancelableTaskTracker cancelable_task_tracker_;
  Profile* profile_;

  base::WeakPtrFactory<NtpIconSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NtpIconSource);
};

#endif  // CHROME_BROWSER_SEARCH_NTP_ICON_SOURCE_H_
