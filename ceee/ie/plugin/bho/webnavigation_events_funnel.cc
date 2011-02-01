// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Funnel of Chrome Extension Events from wherever through the Broker.

#include "ceee/ie/plugin/bho/webnavigation_events_funnel.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_webnavigation_api_constants.h"

namespace keys = extension_webnavigation_api_constants;

namespace {

double MilliSecondsFromTime(const base::Time& time) {
  return base::Time::kMillisecondsPerSecond * time.ToDoubleT();
}

}  // namespace

HRESULT WebNavigationEventsFunnel::OnBeforeNavigate(
    CeeeWindowHandle tab_handle,
    BSTR url,
    int frame_id,
    int request_id,
    const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, static_cast<int>(tab_handle));
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnBeforeNavigate, args);
}

HRESULT WebNavigationEventsFunnel::OnBeforeRetarget(
    CeeeWindowHandle source_tab_handle,
    BSTR source_url,
    BSTR target_url,
    const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kSourceTabIdKey, static_cast<int>(source_tab_handle));
  args.SetString(keys::kSourceUrlKey, source_url);
  args.SetString(keys::kTargetUrlKey, target_url);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnBeforeRetarget, args);
}

HRESULT WebNavigationEventsFunnel::OnCommitted(
    CeeeWindowHandle tab_handle,
    BSTR url,
    int frame_id,
    const char* transition_type,
    const char* transition_qualifiers,
    const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, static_cast<int>(tab_handle));
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetString(keys::kTransitionTypeKey, transition_type);
  args.SetString(keys::kTransitionQualifiersKey, transition_qualifiers);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnCommitted, args);
}

HRESULT WebNavigationEventsFunnel::OnCompleted(
    CeeeWindowHandle tab_handle,
    BSTR url,
    int frame_id,
    const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, static_cast<int>(tab_handle));
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnCompleted, args);
}

HRESULT WebNavigationEventsFunnel::OnDOMContentLoaded(
    CeeeWindowHandle tab_handle,
    BSTR url,
    int frame_id,
    const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, static_cast<int>(tab_handle));
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnDOMContentLoaded, args);
}

HRESULT WebNavigationEventsFunnel::OnErrorOccurred(
    CeeeWindowHandle tab_handle,
    BSTR url,
    int frame_id,
    BSTR error,
    const base::Time& time_stamp) {
  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, static_cast<int>(tab_handle));
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetString(keys::kErrorKey, error);
  args.SetDouble(keys::kTimeStampKey, MilliSecondsFromTime(time_stamp));

  return SendEvent(keys::kOnErrorOccurred, args);
}
