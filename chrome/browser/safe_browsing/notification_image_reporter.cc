// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_image_reporter.h"

#include <cmath>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/url_request/report_sender.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

const size_t kMaxReportsPerDay = 5;
const base::Feature kNotificationImageReporterFeature{
    "NotificationImageReporterFeature", base::FEATURE_ENABLED_BY_DEFAULT};
const char kReportChance[] = "ReportChance";
const char kDefaultMimeType[] = "image/png";

// Passed to ReportSender::Send as an ErrorCallback, so must take a GURL, but it
// is unused.
void LogReportResult(const GURL& url, int net_error) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.NotificationImageReporter.NetError",
                              net_error);
}

}  // namespace

const char NotificationImageReporter::kReportingUploadUrl[] =
    "https://safebrowsing.googleusercontent.com/safebrowsing/clientreport/"
    "notification-image";

NotificationImageReporter::NotificationImageReporter(
    net::URLRequestContext* request_context)
    : NotificationImageReporter(base::MakeUnique<net::ReportSender>(
          request_context,
          net::ReportSender::CookiesPreference::DO_NOT_SEND_COOKIES)) {}

NotificationImageReporter::NotificationImageReporter(
    std::unique_ptr<net::ReportSender> report_sender)
    : report_sender_(std::move(report_sender)), weak_factory_on_io_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

NotificationImageReporter::~NotificationImageReporter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void NotificationImageReporter::ReportNotificationImageOnIO(
    Profile* profile,
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    const GURL& origin,
    const SkBitmap& image) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(profile);
  DCHECK_EQ(origin, origin.GetOrigin());
  DCHECK(origin.is_valid());

  // Skip whitelisted origins to cut down on report volume.
  if (!database_manager || database_manager->MatchCsdWhitelistUrl(origin)) {
    SkippedReporting();
    return;
  }

  // Sample a Finch-controlled fraction only.
  double report_chance = GetReportChance();
  if (base::RandDouble() >= report_chance) {
    SkippedReporting();
    return;
  }

  // Avoid exceeding kMaxReportsPerDay.
  base::Time a_day_ago = base::Time::Now() - base::TimeDelta::FromDays(1);
  while (!report_times_.empty() &&
         report_times_.front() < /* older than */ a_day_ago) {
    report_times_.pop();
  }
  if (report_times_.size() >= kMaxReportsPerDay) {
    SkippedReporting();
    return;
  }
  // n.b. we write to report_times_ here even if we'll later end up skipping
  // reporting because GetExtendedReportingLevel was not SBER_LEVEL_SCOUT. That
  // saves us two thread hops, with the downside that we may underreport
  // notifications on the first day that a user opts in to SBER_LEVEL_SCOUT.
  report_times_.push(base::Time::Now());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotificationImageReporter::ReportNotificationImageOnUI,
                 weak_factory_on_io_.GetWeakPtr(), profile, origin, image));
}

double NotificationImageReporter::GetReportChance() const {
  // Get the report_chance from the Finch experiment. If there is no active
  // experiment, it will be set to the default of 0.
  double report_chance = variations::GetVariationParamByFeatureAsDouble(
      kNotificationImageReporterFeature, kReportChance, 0.0);

  if (report_chance < 0.0 || report_chance > 1.0) {
    DLOG(WARNING) << "Illegal value " << report_chance << " for the parameter "
                  << kReportChance << ". The value should be between 0 and 1.";
    report_chance = 0.0;
  }

  return report_chance;
}

void NotificationImageReporter::SkippedReporting() {}

// static
void NotificationImageReporter::ReportNotificationImageOnUI(
    const base::WeakPtr<NotificationImageReporter>& weak_this_on_io,
    Profile* profile,
    const GURL& origin,
    const SkBitmap& image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Skip reporting unless SBER2 Scout is enabled.
  if (GetExtendedReportingLevel(*profile->GetPrefs()) != SBER_LEVEL_SCOUT) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&NotificationImageReporter::SkippedReporting,
                   weak_this_on_io));
    return;
  }

  BrowserThread::GetBlockingPool()->PostWorkerTask(
      FROM_HERE,
      base::Bind(
          &NotificationImageReporter::DownscaleNotificationImageOnBlockingPool,
          weak_this_on_io, origin, image));
}

// static
void NotificationImageReporter::DownscaleNotificationImageOnBlockingPool(
    const base::WeakPtr<NotificationImageReporter>& weak_this_on_io,
    const GURL& origin,
    const SkBitmap& image) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  // Downscale to fit within 512x512. TODO(johnme): Get this from Finch.
  const double MAX_SIZE = 512;
  SkBitmap downscaled_image = image;
  if ((image.width() > MAX_SIZE || image.height() > MAX_SIZE) &&
      image.width() > 0 && image.height() > 0) {
    double scale =
        std::min(MAX_SIZE / image.width(), MAX_SIZE / image.height());
    downscaled_image =
        skia::ImageOperations::Resize(image, skia::ImageOperations::RESIZE_GOOD,
                                      std::lround(scale * image.width()),
                                      std::lround(scale * image.height()));
  }

  // Encode as PNG.
  std::vector<unsigned char> png_bytes;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(downscaled_image, false, &png_bytes)) {
    NOTREACHED();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotificationImageReporter::SendReportOnIO, weak_this_on_io,
                 origin, base::RefCountedBytes::TakeVector(&png_bytes),
                 gfx::Size(downscaled_image.width(), downscaled_image.height()),
                 gfx::Size(image.width(), image.height())));
}

void NotificationImageReporter::SendReportOnIO(
    const GURL& origin,
    scoped_refptr<base::RefCountedMemory> data,
    const gfx::Size& dimensions,
    const gfx::Size& original_dimensions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  NotificationImageReportRequest report;
  report.set_notification_origin(origin.spec());
  report.mutable_image()->set_data(data->front(), data->size());
  report.mutable_image()->set_mime_type(kDefaultMimeType);
  report.mutable_image()->mutable_dimensions()->set_width(dimensions.width());
  report.mutable_image()->mutable_dimensions()->set_height(dimensions.height());
  if (dimensions != original_dimensions) {
    report.mutable_image()->mutable_original_dimensions()->set_width(
        original_dimensions.width());
    report.mutable_image()->mutable_original_dimensions()->set_height(
        original_dimensions.height());
  }

  std::string serialized_report;
  report.SerializeToString(&serialized_report);
  report_sender_->Send(
      GURL(kReportingUploadUrl), "application/octet-stream", serialized_report,
      base::Bind(&LogReportResult, GURL(kReportingUploadUrl), net::OK),
      base::Bind(&LogReportResult));
  // TODO(johnme): Consider logging bandwidth and/or duration to UMA.
}

}  // namespace safe_browsing
