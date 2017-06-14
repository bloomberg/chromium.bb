// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filtering_info.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace extensions {

namespace {

const char kInstanceId[] = "instanceId";
const char kServiceType[] = "serviceType";
const char kWindowType[] = "windowType";
const char kWindowExposedByDefault[] = "windowExposedByDefault";

}

EventFilteringInfo::EventFilteringInfo() {}

EventFilteringInfo::EventFilteringInfo(const base::DictionaryValue& dict)
    : EventFilteringInfo() {
  {
    std::string dict_url;
    if (dict.GetString("url", &dict_url)) {
      GURL maybe_url(dict_url);
      if (maybe_url.is_valid())
        url = std::move(maybe_url);
    }
  }

  {
    int dict_instance_id = 0;
    if (dict.GetInteger(kInstanceId, &dict_instance_id))
      instance_id = dict_instance_id;
  }

  {
    std::string dict_service_type;
    if (dict.GetString(kServiceType, &dict_service_type))
      service_type = std::move(dict_service_type);
  }

  {
    std::string dict_window_type;
    if (dict.GetString(kWindowType, &dict_window_type))
      window_type = std::move(dict_window_type);
  }

  {
    bool dict_window_exposed_by_default = false;
    if (dict.GetBoolean(kWindowExposedByDefault,
                        &dict_window_exposed_by_default))
      window_exposed_by_default = dict_window_exposed_by_default;
  }
}

EventFilteringInfo::EventFilteringInfo(const EventFilteringInfo& other) =
    default;

EventFilteringInfo::~EventFilteringInfo() {}

std::unique_ptr<base::DictionaryValue> EventFilteringInfo::AsValue() const {
  auto result = base::MakeUnique<base::DictionaryValue>();
  if (url)
    result->SetString("url", url->spec());

  if (instance_id)
    result->SetInteger(kInstanceId, *instance_id);

  if (service_type)
    result->SetString(kServiceType, *service_type);

  if (window_type)
    result->SetString(kWindowType, *window_type);

  if (window_exposed_by_default)
    result->SetBoolean(kWindowExposedByDefault, *window_exposed_by_default);

  return result;
}

}  // namespace extensions
