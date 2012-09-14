// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_intents_reporting.h"

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "net/base/mime_util.h"
#include "webkit/glue/web_intent_data.h"

namespace web_intents {
namespace {

struct TypeMapping {
  const char* name;
  const TypeId id;
};

const TypeMapping kTypeMap[] = {
  { "application", TYPE_ID_APPLICATION },
  { "audio", TYPE_ID_AUDIO },
  { "example", TYPE_ID_EXAMPLE },
  { "image", TYPE_ID_IMAGE },
  { "message", TYPE_ID_MESSAGE },
  { "model", TYPE_ID_MODEL },
  { "multipart", TYPE_ID_MULTIPART },
  { "text", TYPE_ID_TEXT },
  { "video", TYPE_ID_VIDEO },
};

// The number of buckets for the histogram of the duration of time spent in a
// service.
const int kServiceActiveTimeNumBuckets = 10;

// The lower bound on the tracked duration of time spent in a service.
// This bound is in seconds.
const int kServiceActiveDurationMinSeconds = 1;

// The upper bound on the tracked duration of time spent in a service.
// This bound is in seconds.
const int kServiceActiveDurationMaxSeconds = 3600;

// Returns the ActionMapping for |action| if one exists, or NULL.
TypeId ToTypeId(const string16& type) {
  const std::string iana_type = net::GetIANAMediaType(UTF16ToASCII(type));
  for (size_t i = 0; i < arraysize(kTypeMap); ++i) {
    if (iana_type == kTypeMap[i].name) {
      return kTypeMap[i].id;
    }
  }
  return TYPE_ID_CUSTOM;
}

// Records the number of services installed at the time the picker
// is shown to the user. Drops the size into one of several buckets.
void RecordInstalledServiceCount(const UMABucket bucket, size_t installed) {
  if (installed == 0) {
    UMA_HISTOGRAM_ENUMERATION("WebIntents.Service.NumInstalled.0.v0",
        bucket, kMaxActionTypeHistogramValue);
  } else if (installed >= 1 && installed <= 4) {
    UMA_HISTOGRAM_ENUMERATION("WebIntents.Service.NumInstalled.1-4.v0",
        bucket, kMaxActionTypeHistogramValue);
  } else if (installed >= 5 && installed <= 8) {
    UMA_HISTOGRAM_ENUMERATION("WebIntents.Service.NumInstalled.5-8.v0",
        bucket, kMaxActionTypeHistogramValue);
  } else {
    UMA_HISTOGRAM_ENUMERATION("WebIntents.Service.NumInstalled.9+.v0",
        bucket, kMaxActionTypeHistogramValue);
  }
}

}  // namespace

UMABucket ToUMABucket(const string16& action, const string16& type) {
  ActionId action_id = ToActionId(action);
  TypeId type_id = ToTypeId(type);
  short bucket_id = (action_id << 8) | type_id;
  DCHECK(bucket_id > 256);
  return static_cast<UMABucket>(bucket_id);
}

void RecordIntentsDispatchDisabled() {
  UMA_HISTOGRAM_COUNTS("WebIntents.DispatchDisabled", 1);
}

void RecordIntentDispatchRequested() {
  UMA_HISTOGRAM_COUNTS("WebIntents.Dispatch", 1);
}

void RecordIntentDispatched(const UMABucket bucket) {
  UMA_HISTOGRAM_ENUMERATION("WebIntents.IntentDispatched.v0",
      bucket, kMaxActionTypeHistogramValue);
}

void RecordPickerShow(const UMABucket bucket, size_t installed) {
  UMA_HISTOGRAM_ENUMERATION("WebIntents.Picker.Show.v0",
      bucket, kMaxActionTypeHistogramValue);
  RecordInstalledServiceCount(bucket, installed);
}

void RecordPickerCancel(const UMABucket bucket) {
  UMA_HISTOGRAM_ENUMERATION("WebIntents.Picker.Cancel.v0",
      bucket, kMaxActionTypeHistogramValue);
}

void RecordServiceInvoke(const UMABucket bucket) {
  UMA_HISTOGRAM_ENUMERATION("WebIntents.Service.Invoked.v0",
      bucket, kMaxActionTypeHistogramValue);
}

void RecordChooseAnotherService(const UMABucket bucket) {
  UMA_HISTOGRAM_ENUMERATION("WebIntents.Service.ChooseAnother.v0",
      bucket, kMaxActionTypeHistogramValue);
}

void RecordCWSExtensionInstalled(const UMABucket bucket) {
  UMA_HISTOGRAM_ENUMERATION("WebIntents.Service.CWSInstall.v0",
      bucket, kMaxActionTypeHistogramValue);
}

void RecordServiceActiveDuration(
    webkit_glue::WebIntentReplyType reply_type,
    const base::TimeDelta& duration) {
  switch (reply_type) {
    case webkit_glue::WEB_INTENT_REPLY_SUCCESS:
      UMA_HISTOGRAM_CUSTOM_TIMES("WebIntents.Service.ActiveDuration.Success",
          duration,
          base::TimeDelta::FromSeconds(kServiceActiveDurationMinSeconds),
          base::TimeDelta::FromSeconds(kServiceActiveDurationMaxSeconds),
          kServiceActiveTimeNumBuckets);
      break;
    case webkit_glue::WEB_INTENT_REPLY_INVALID:
    case webkit_glue::WEB_INTENT_REPLY_FAILURE:
    case webkit_glue::WEB_INTENT_PICKER_CANCELLED:
    case webkit_glue::WEB_INTENT_SERVICE_CONTENTS_CLOSED:
      UMA_HISTOGRAM_CUSTOM_TIMES("WebIntents.Service.ActiveDuration.Failure",
          duration,
          base::TimeDelta::FromSeconds(kServiceActiveDurationMinSeconds),
          base::TimeDelta::FromSeconds(kServiceActiveDurationMaxSeconds),
          kServiceActiveTimeNumBuckets);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace web_intents
