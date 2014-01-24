// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_uma_host.h"

#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace {

const char* kPredefinedAllowedUMAOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see crbug.com/317833
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"   // see crbug.com/317833
};

const char* kWhitelistedHistogramHashes[] = {
  "F131550DAB7A7C6E6633EF81FB5998CC0482AC63",  // see crbug.com/317833
  "13955AB4DAD798384DFB4304734FCF2A95F353CC",  // see crbug.com/317833
  "404E800582901F1B937B8E287235FC603A5DEDFB"   // see crbug.com/317833
};

std::string HashHistogram(const std::string& histogram) {
  const std::string id_hash = base::SHA1HashString(histogram);
  DCHECK_EQ(id_hash.length(), base::kSHA1Length);
  return base::HexEncode(id_hash.c_str(), id_hash.length());
}

}  // namespace

PepperUMAHost::PepperUMAHost(
    content::RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      document_url_(host->GetDocumentURL(instance)),
      is_plugin_in_process_(host->IsRunningInProcess()) {
  for (size_t i = 0; i < arraysize(kPredefinedAllowedUMAOrigins); ++i)
    allowed_origins_.insert(kPredefinedAllowedUMAOrigins[i]);
  for (size_t i = 0; i < arraysize(kWhitelistedHistogramHashes); ++i)
    allowed_histograms_.insert(kWhitelistedHistogramHashes[i]);
}

PepperUMAHost::~PepperUMAHost() {
}

int32_t PepperUMAHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperUMAHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UMA_HistogramCustomTimes,
        OnHistogramCustomTimes);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UMA_HistogramCustomCounts,
        OnHistogramCustomCounts);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UMA_HistogramEnumeration,
        OnHistogramEnumeration);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperUMAHost::IsHistogramAllowed(const std::string& histogram) {
  if (is_plugin_in_process_ && histogram.find("NaCl.") == 0) {
    return true;
  }

  bool is_whitelisted =
      ChromeContentRendererClient::IsExtensionOrSharedModuleWhitelisted(
          document_url_, allowed_origins_);
  if (is_whitelisted &&
      allowed_histograms_.find(HashHistogram(histogram)) !=
          allowed_histograms_.end()) {
    return true;
  }

  LOG(ERROR) << "Host or histogram name is not allowed to use the UMA API.";
  return false;
}

#define RETURN_IF_BAD_ARGS(_min, _max, _buckets) \
  do { \
    if (_min >= _max || _buckets <= 1) \
      return PP_ERROR_BADARGUMENT; \
  } while (0)

int32_t PepperUMAHost::OnHistogramCustomTimes(
    ppapi::host::HostMessageContext* context,
    const std::string& name,
    int64_t sample,
    int64_t min,
    int64_t max,
    uint32_t bucket_count) {
  if (!IsHistogramAllowed(name)) {
    return PP_ERROR_NOACCESS;
  }
  RETURN_IF_BAD_ARGS(min, max, bucket_count);

  base::HistogramBase* counter =
      base::Histogram::FactoryTimeGet(
          name,
          base::TimeDelta::FromMilliseconds(min),
          base::TimeDelta::FromMilliseconds(max),
          bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddTime(base::TimeDelta::FromMilliseconds(sample));
  return PP_OK;
}

int32_t PepperUMAHost::OnHistogramCustomCounts(
    ppapi::host::HostMessageContext* context,
    const std::string& name,
    int32_t sample,
    int32_t min,
    int32_t max,
    uint32_t bucket_count) {
  if (!IsHistogramAllowed(name)) {
    return PP_ERROR_NOACCESS;
  }
  RETURN_IF_BAD_ARGS(min, max, bucket_count);

  base::HistogramBase* counter =
      base::Histogram::FactoryGet(
          name,
          min,
          max,
          bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
  return PP_OK;
}

int32_t PepperUMAHost::OnHistogramEnumeration(
    ppapi::host::HostMessageContext* context,
    const std::string& name,
    int32_t sample,
    int32_t boundary_value) {
  if (!IsHistogramAllowed(name)) {
    return PP_ERROR_NOACCESS;
  }
  RETURN_IF_BAD_ARGS(0, boundary_value, boundary_value + 1);

  base::HistogramBase* counter =
      base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
  return PP_OK;
}

