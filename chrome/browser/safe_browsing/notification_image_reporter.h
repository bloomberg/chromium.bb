// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_IMAGE_REPORTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_IMAGE_REPORTER_H_

#include <memory>
#include <queue>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class GURL;
class Profile;
class SkBitmap;

namespace base {
class RefCountedMemory;
}  // namespace base

namespace gfx {
class Size;
}  // namespace gfx

namespace net {
class ReportSender;
class URLRequestContext;
}  // namespace net

namespace safe_browsing {

class SafeBrowsingDatabaseManager;

// Provides functionality for building and sending reports about notification
// content images to the Safe Browsing CSD server.
class NotificationImageReporter {
 public:
  // CSD server URL to which notification image reports are sent.
  static const char kReportingUploadUrl[];

  explicit NotificationImageReporter(net::URLRequestContext* request_context);
  virtual ~NotificationImageReporter();

  // Report notification content image to SafeBrowsing CSD server if the user
  // has opted in, the origin is not whitelisted, and it is randomly sampled.
  // Can only be called on IO thread.
  void ReportNotificationImageOnIO(
      Profile* profile,
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      const GURL& origin,
      const SkBitmap& image);

 protected:
  explicit NotificationImageReporter(
      std::unique_ptr<net::ReportSender> report_sender);

  // Get the percentage of images that should be reported from Finch.
  virtual double GetReportChance() const;

  // Tests may wish to override this to find out if reports are skipped. Called
  // on the IO thread.
  virtual void SkippedReporting();

 private:
  // Report notification content image to SafeBrowsing CSD server if necessary,
  // by invoking DownscaleNotificationImageOnBlockingPool. This should be
  // called on the IO thread and will invoke a member function on the IO thread
  // using the IO-based WeapPtr.
  static void ReportNotificationImageOnUI(
      const base::WeakPtr<NotificationImageReporter>& weak_this_on_io,
      Profile* profile,
      const GURL& origin,
      const SkBitmap& image);

  // Downscales image to fit within 512x512 if necessary, and encodes as it PNG,
  // then invokes SendReportOnIO. This should be called on a blocking pool
  // thread and will invoke a member function on the IO thread using the
  // IO-based WeakPtr.
  static void DownscaleNotificationImageOnBlockingPool(
      const base::WeakPtr<NotificationImageReporter>& weak_this_on_io,
      const GURL& origin,
      const SkBitmap& image);

  // Serializes report using NotificationImageReportRequest protobuf defined in
  // chrome/common/safe_browsing/csd.proto and sends it to CSD server.
  void SendReportOnIO(const GURL& origin,
                      scoped_refptr<base::RefCountedMemory> data,
                      const gfx::Size& dimensions,
                      const gfx::Size& original_dimensions);

  std::unique_ptr<net::ReportSender> report_sender_;

  // Timestamps of when we sent notification images. Used to limit the number
  // of requests that we send in a day. Only access on the IO thread.
  // TODO(johnme): Serialize this so that it doesn't reset on browser restart.
  std::queue<base::Time> report_times_;

  // Keep this last. Only dereference these pointers on the IO thread.
  base::WeakPtrFactory<NotificationImageReporter> weak_factory_on_io_;

  DISALLOW_COPY_AND_ASSIGN(NotificationImageReporter);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_IMAGE_REPORTER_H_
