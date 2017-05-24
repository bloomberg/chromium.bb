// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_PUBLIC_UKM_RECORDER_H_
#define COMPONENTS_UKM_PUBLIC_UKM_RECORDER_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/ukm/public/interfaces/ukm_interface.mojom.h"
#include "components/ukm/public/ukm_entry_builder.h"
#include "components/ukm/public/ukm_export.h"
#include "url/gurl.h"

class ContextualSearchRankerLoggerImpl;
class PluginInfoMessageFilter;
class UkmPageLoadMetricsObserver;

namespace autofill {
class AutofillMetrics;
}

namespace content {
class MediaInternals;
class RenderFrameImpl;
}

namespace translate {
class TranslateRankerImpl;
}

namespace payments {
class JourneyLogger;
}

namespace ukm {

class UkmEntryBuilder;
class UkmInterface;
class TestRecordingHelper;

// This feature controls whether UkmService should be created.
UKM_EXPORT extern const base::Feature kUkmFeature;

typedef int64_t SourceId;

// Interface for recording UKM
class UKM_EXPORT UkmRecorder {
 public:
  UkmRecorder();
  virtual ~UkmRecorder();

  // Provides access to a previously constructed UkmRecorder instance. Only one
  // instance exists per process and must have been constructed prior to any
  // calls to this method.
  static UkmRecorder* Get();

  // Get the new source ID, which is unique for the duration of a browser
  // session.
  static SourceId GetNewSourceID();

  // Update the URL on the source keyed to the given source ID. If the source
  // does not exist, it will create a new UkmSource object.
  virtual void UpdateSourceURL(SourceId source_id, const GURL& url) = 0;

 private:
  friend autofill::AutofillMetrics;
  friend payments::JourneyLogger;
  friend ContextualSearchRankerLoggerImpl;
  friend PluginInfoMessageFilter;
  friend UkmPageLoadMetricsObserver;
  friend translate::TranslateRankerImpl;
  friend TestRecordingHelper;
  friend UkmInterface;
  friend content::MediaInternals;
  friend content::RenderFrameImpl;
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, AddEntryWithEmptyMetrics);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, EntryBuilderAndSerialization);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest,
                           LogsUploadedOnlyWhenHavingSourcesOrEntries);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, MetricsProviderTest);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, PersistAndPurge);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, WhitelistEntryTest);

  // Get a new UkmEntryBuilder object for the specified source ID and event,
  // which can get metrics added to.
  //
  // This API being private is intentional. Any client using UKM needs to
  // declare itself to be a friend of UkmService and go through code review
  // process.
  std::unique_ptr<UkmEntryBuilder> GetEntryBuilder(SourceId source_id,
                                                   const char* event_name);

 private:
  // Add an entry to the UkmEntry list.
  virtual void AddEntry(mojom::UkmEntryPtr entry) = 0;

  DISALLOW_COPY_AND_ASSIGN(UkmRecorder);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_PUBLIC_UKM_RECORDER_H_
