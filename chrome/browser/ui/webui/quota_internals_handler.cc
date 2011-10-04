// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/quota_internals_proxy.h"
#include "chrome/browser/ui/webui/quota_internals_types.h"
#include "net/base/net_util.h"

namespace quota_internals {

QuotaInternalsHandler::QuotaInternalsHandler() {}

QuotaInternalsHandler::~QuotaInternalsHandler() {
  if (proxy_)
    proxy_->handler_ = NULL;
}

void QuotaInternalsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("requestInfo",
      base::Bind(&QuotaInternalsHandler::OnRequestInfo,
                 base::Unretained(this)));
}

void QuotaInternalsHandler::ReportAvailableSpace(int64 available_space) {
  scoped_ptr<base::Value> avail(
      base::Value::CreateDoubleValue(static_cast<double>(available_space)));
  SendMessage("AvailableSpaceUpdated", *avail);
}

void QuotaInternalsHandler::ReportGlobalInfo(const GlobalStorageInfo& data) {
  scoped_ptr<base::Value> value(data.NewValue());
  SendMessage("GlobalInfoUpdated", *value);
}

void QuotaInternalsHandler::ReportPerHostInfo(
    const std::vector<PerHostStorageInfo>& hosts) {
  base::ListValue values;
  typedef std::vector<PerHostStorageInfo>::const_iterator iterator;
  for (iterator itr(hosts.begin()); itr != hosts.end(); ++itr) {
    values.Append(itr->NewValue());
  }

  SendMessage("PerHostInfoUpdated", values);
}

void QuotaInternalsHandler::ReportPerOriginInfo(
    const std::vector<PerOriginStorageInfo>& origins) {
  base::ListValue origins_value;
  typedef std::vector<PerOriginStorageInfo>::const_iterator iterator;
  for (iterator itr(origins.begin()); itr != origins.end(); ++itr) {
    origins_value.Append(itr->NewValue());
  }

  SendMessage("PerOriginInfoUpdated", origins_value);
}

void QuotaInternalsHandler::ReportStatistics(const Statistics& stats) {
  base::DictionaryValue dict;
  typedef Statistics::const_iterator iterator;
  for (iterator itr(stats.begin()); itr != stats.end(); ++itr) {
    dict.SetString(itr->first, itr->second);
  }

  SendMessage("StatisticsUpdated", dict);
}

void QuotaInternalsHandler::SendMessage(const std::string& message,
                                        const base::Value& value) {
  scoped_ptr<base::Value> message_data(base::Value::CreateStringValue(message));
  web_ui_->CallJavascriptFunction("cr.quota.messageHandler",
                                  *message_data,
                                  value);
}

void QuotaInternalsHandler::OnRequestInfo(const base::ListValue*) {
  if (!proxy_)
    proxy_ = new QuotaInternalsProxy(this);
  proxy_->RequestInfo(Profile::FromWebUI(web_ui_)->GetQuotaManager());
}

}  // namespace quota_internals
