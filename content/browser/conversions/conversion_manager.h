// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_H_

#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "content/browser/conversions/conversion_policy.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class WebContents;

// Interface that mediates data flow between the network, storage layer, and
// blink.
class CONTENT_EXPORT ConversionManager {
 public:
  // Provides access to a ConversionManager implementation. This layer of
  // abstraction is to allow tests to mock out the ConversionManager without
  // injecting a manager explicitly.
  class Provider {
   public:
    virtual ~Provider() = default;

    // Gets the ConversionManager that should be used for handling conversions
    // that occur in the given |web_contents|. Returns nullptr if conversion
    // measurement is not enabled in the given |web_contents|, e.g. when the
    // browser context is off the record.
    virtual ConversionManager* GetManager(WebContents* web_contents) const = 0;
  };
  virtual ~ConversionManager() = default;

  // Persists the given |impression| to storage. Called when a navigation
  // originating from an impression tag finishes.
  virtual void HandleImpression(const StorableImpression& impression) = 0;

  // Process a newly registered conversion. Will create and log any new
  // conversion reports to storage.
  virtual void HandleConversion(const StorableConversion& conversion) = 0;

  // Get all impressions that are currently stored in this partition. Used for
  // populating WebUI.
  virtual void GetActiveImpressionsForWebUI(
      base::OnceCallback<void(std::vector<StorableImpression>)> callback) = 0;

  // Get all pending reports that are currently stored in this partition. Used
  // for populating WebUI.
  virtual void GetReportsForWebUI(
      base::OnceCallback<void(std::vector<ConversionReport>)> callback,
      base::Time max_report_time) = 0;

  // Sends all pending reports immediately, and runs |done| once they have all
  // been sent.
  virtual void SendReportsForWebUI(base::OnceClosure done) = 0;

  // Returns the ConversionPolicy that is used to control API policies such
  // as noise.
  virtual const ConversionPolicy& GetConversionPolicy() const = 0;

  // Deletes all data in storage for URLs matching |filter|, between
  // |delete_begin| and |delete_end| time.
  //
  // If |filter| is null, then consider all origins in storage as matching.
  virtual void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter,
      base::OnceClosure done) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_MANAGER_H_
