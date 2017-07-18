// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/download/internal/proto_conversions.h"
#include "net/http/http_request_headers.h"

namespace download {

protodb::Entry_State ProtoConversions::RequestStateToProto(Entry::State state) {
  switch (state) {
    case Entry::State::NEW:
      return protodb::Entry_State_NEW;
    case Entry::State::AVAILABLE:
      return protodb::Entry_State_AVAILABLE;
    case Entry::State::ACTIVE:
      return protodb::Entry_State_ACTIVE;
    case Entry::State::PAUSED:
      return protodb::Entry_State_PAUSED;
    case Entry::State::COMPLETE:
      return protodb::Entry_State_COMPLETE;
    case Entry::State::COUNT:
      break;
  }

  NOTREACHED();
  return protodb::Entry_State_NEW;
}

Entry::State ProtoConversions::RequestStateFromProto(
    protodb::Entry_State state) {
  switch (state) {
    case protodb::Entry_State_NEW:
      return Entry::State::NEW;
    case protodb::Entry_State_AVAILABLE:
      return Entry::State::AVAILABLE;
    case protodb::Entry_State_ACTIVE:
      return Entry::State::ACTIVE;
    case protodb::Entry_State_PAUSED:
      return Entry::State::PAUSED;
    case protodb::Entry_State_COMPLETE:
      return Entry::State::COMPLETE;
  }

  NOTREACHED();
  return Entry::State::NEW;
}

protodb::DownloadClient ProtoConversions::DownloadClientToProto(
    DownloadClient client) {
  switch (client) {
    case DownloadClient::INVALID:
      return protodb::DownloadClient::INVALID;
    case DownloadClient::TEST:
      return protodb::DownloadClient::TEST;
    case DownloadClient::TEST_2:
      return protodb::DownloadClient::TEST_2;
    case DownloadClient::TEST_3:
      return protodb::DownloadClient::TEST_3;
    case DownloadClient::OFFLINE_PAGE_PREFETCH:
      return protodb::DownloadClient::OFFLINE_PAGE_PREFETCH;
    case DownloadClient::BOUNDARY:
      return protodb::DownloadClient::BOUNDARY;
  }

  NOTREACHED();
  return protodb::DownloadClient::INVALID;
}

DownloadClient ProtoConversions::DownloadClientFromProto(
    protodb::DownloadClient client) {
  switch (client) {
    case protodb::DownloadClient::INVALID:
      return DownloadClient::INVALID;
    case protodb::DownloadClient::TEST:
      return DownloadClient::TEST;
    case protodb::DownloadClient::TEST_2:
      return DownloadClient::TEST_2;
    case protodb::DownloadClient::TEST_3:
      return DownloadClient::TEST_3;
    case protodb::DownloadClient::OFFLINE_PAGE_PREFETCH:
      return DownloadClient::OFFLINE_PAGE_PREFETCH;
    case protodb::DownloadClient::BOUNDARY:
      return DownloadClient::BOUNDARY;
  }

  NOTREACHED();
  return DownloadClient::INVALID;
}

SchedulingParams::NetworkRequirements
ProtoConversions::NetworkRequirementsFromProto(
    protodb::SchedulingParams_NetworkRequirements network_requirements) {
  switch (network_requirements) {
    case protodb::SchedulingParams_NetworkRequirements_NONE:
      return SchedulingParams::NetworkRequirements::NONE;
    case protodb::SchedulingParams_NetworkRequirements_OPTIMISTIC:
      return SchedulingParams::NetworkRequirements::OPTIMISTIC;
    case protodb::SchedulingParams_NetworkRequirements_UNMETERED:
      return SchedulingParams::NetworkRequirements::UNMETERED;
  }

  NOTREACHED();
  return SchedulingParams::NetworkRequirements::NONE;
}

protodb::SchedulingParams_NetworkRequirements
ProtoConversions::NetworkRequirementsToProto(
    SchedulingParams::NetworkRequirements network_requirements) {
  switch (network_requirements) {
    case SchedulingParams::NetworkRequirements::NONE:
      return protodb::SchedulingParams_NetworkRequirements_NONE;
    case SchedulingParams::NetworkRequirements::OPTIMISTIC:
      return protodb::SchedulingParams_NetworkRequirements_OPTIMISTIC;
    case SchedulingParams::NetworkRequirements::UNMETERED:
      return protodb::SchedulingParams_NetworkRequirements_UNMETERED;
    case SchedulingParams::NetworkRequirements::COUNT:
      break;
  }

  NOTREACHED();
  return protodb::SchedulingParams_NetworkRequirements_NONE;
}

SchedulingParams::BatteryRequirements
ProtoConversions::BatteryRequirementsFromProto(
    protodb::SchedulingParams_BatteryRequirements battery_requirements) {
  switch (battery_requirements) {
    case protodb::SchedulingParams_BatteryRequirements_BATTERY_INSENSITIVE:
      return SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
    case protodb::SchedulingParams_BatteryRequirements_BATTERY_SENSITIVE:
      return SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
  }

  NOTREACHED();
  return SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
}

protodb::SchedulingParams_BatteryRequirements
ProtoConversions::BatteryRequirementsToProto(
    SchedulingParams::BatteryRequirements battery_requirements) {
  switch (battery_requirements) {
    case SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE:
      return protodb::SchedulingParams_BatteryRequirements_BATTERY_INSENSITIVE;
    case SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE:
      return protodb::SchedulingParams_BatteryRequirements_BATTERY_SENSITIVE;
    case SchedulingParams::BatteryRequirements::COUNT:
      break;
  }

  NOTREACHED();
  return protodb::SchedulingParams_BatteryRequirements_BATTERY_INSENSITIVE;
}

SchedulingParams::Priority ProtoConversions::SchedulingPriorityFromProto(
    protodb::SchedulingParams_Priority priority) {
  switch (priority) {
    case protodb::SchedulingParams_Priority_LOW:
      return SchedulingParams::Priority::LOW;
    case protodb::SchedulingParams_Priority_NORMAL:
      return SchedulingParams::Priority::NORMAL;
    case protodb::SchedulingParams_Priority_HIGH:
      return SchedulingParams::Priority::HIGH;
    case protodb::SchedulingParams_Priority_UI:
      return SchedulingParams::Priority::UI;
  }

  NOTREACHED();
  return SchedulingParams::Priority::LOW;
}

protodb::SchedulingParams_Priority ProtoConversions::SchedulingPriorityToProto(
    SchedulingParams::Priority priority) {
  switch (priority) {
    case SchedulingParams::Priority::LOW:
      return protodb::SchedulingParams_Priority_LOW;
    case SchedulingParams::Priority::NORMAL:
      return protodb::SchedulingParams_Priority_NORMAL;
    case SchedulingParams::Priority::HIGH:
      return protodb::SchedulingParams_Priority_HIGH;
    case SchedulingParams::Priority::UI:
      return protodb::SchedulingParams_Priority_UI;
    case SchedulingParams::Priority::COUNT:
      break;
  }

  NOTREACHED();
  return protodb::SchedulingParams_Priority_LOW;
}

SchedulingParams ProtoConversions::SchedulingParamsFromProto(
    const protodb::SchedulingParams& proto) {
  SchedulingParams scheduling_params;

  scheduling_params.cancel_time =
      base::Time::FromInternalValue(proto.cancel_time());
  scheduling_params.priority = SchedulingPriorityFromProto(proto.priority());
  scheduling_params.network_requirements =
      NetworkRequirementsFromProto(proto.network_requirements());
  scheduling_params.battery_requirements =
      BatteryRequirementsFromProto(proto.battery_requirements());

  return scheduling_params;
}

void ProtoConversions::SchedulingParamsToProto(
    const SchedulingParams& scheduling_params,
    protodb::SchedulingParams* proto) {
  proto->set_cancel_time(scheduling_params.cancel_time.ToInternalValue());
  proto->set_priority(SchedulingPriorityToProto(scheduling_params.priority));
  proto->set_network_requirements(
      NetworkRequirementsToProto(scheduling_params.network_requirements));
  proto->set_battery_requirements(
      BatteryRequirementsToProto(scheduling_params.battery_requirements));
}

RequestParams ProtoConversions::RequestParamsFromProto(
    const protodb::RequestParams& proto) {
  RequestParams request_params;
  request_params.url = GURL(proto.url());
  request_params.method = proto.method();

  for (int i = 0; i < proto.headers_size(); i++) {
    protodb::RequestHeader header = proto.headers(i);
    request_params.request_headers.SetHeader(header.key(), header.value());
  }

  return request_params;
}

void ProtoConversions::RequestParamsToProto(const RequestParams& request_params,
                                            protodb::RequestParams* proto) {
  proto->set_url(request_params.url.spec());
  proto->set_method(request_params.method);

  int i = 0;
  net::HttpRequestHeaders::Iterator iter(request_params.request_headers);
  while (iter.GetNext()) {
    protodb::RequestHeader* header = proto->add_headers();
    header->set_key(iter.name());
    header->set_value(iter.value());
    i++;
  }
}

Entry ProtoConversions::EntryFromProto(const protodb::Entry& proto) {
  Entry entry;

  entry.guid = proto.guid();
  entry.client = DownloadClientFromProto(proto.name_space());
  entry.scheduling_params =
      SchedulingParamsFromProto(proto.scheduling_params());
  entry.request_params = RequestParamsFromProto(proto.request_params());
  entry.state = RequestStateFromProto(proto.state());
  entry.target_file_path =
      base::FilePath::FromUTF8Unsafe(proto.target_file_path());
  entry.create_time = base::Time::FromInternalValue(proto.create_time());
  entry.completion_time =
      base::Time::FromInternalValue(proto.completion_time());
  entry.attempt_count = proto.attempt_count();

  return entry;
}

protodb::Entry ProtoConversions::EntryToProto(const Entry& entry) {
  protodb::Entry proto;

  proto.set_guid(entry.guid);
  proto.set_name_space(DownloadClientToProto(entry.client));
  SchedulingParamsToProto(entry.scheduling_params,
                          proto.mutable_scheduling_params());
  RequestParamsToProto(entry.request_params, proto.mutable_request_params());
  proto.set_state(RequestStateToProto(entry.state));
  proto.set_target_file_path(entry.target_file_path.AsUTF8Unsafe());
  proto.set_create_time(entry.create_time.ToInternalValue());
  proto.set_completion_time(entry.completion_time.ToInternalValue());
  proto.set_attempt_count(entry.attempt_count);

  return proto;
}

std::unique_ptr<std::vector<Entry>> ProtoConversions::EntryVectorFromProto(
    std::unique_ptr<std::vector<protodb::Entry>> protos) {
  auto entries = base::MakeUnique<std::vector<Entry>>();
  for (auto& proto : *protos) {
    entries->push_back(EntryFromProto(proto));
  }

  return entries;
}

std::unique_ptr<std::vector<protodb::Entry>>
ProtoConversions::EntryVectorToProto(
    std::unique_ptr<std::vector<Entry>> entries) {
  auto protos = base::MakeUnique<std::vector<protodb::Entry>>();
  for (auto& entry : *entries) {
    protos->push_back(EntryToProto(entry));
  }

  return protos;
}

}  // namespace download
